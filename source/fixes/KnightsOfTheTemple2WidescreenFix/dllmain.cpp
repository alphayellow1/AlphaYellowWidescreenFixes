// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>
#include <vector>
#include <map>
#include <windows.h>
#include <psapi.h> // For GetModuleInformation
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "KnightsOfTheTemple2WidescreenFix";
std::string sFixVersion = "1.0";
std::filesystem::path sFixPath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fTolerance = 0.0001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraHFOV;
float fNewAspectRatio;
float fNewAspectRatio2;
float fFOVFactor;
float fNewGameplayCameraFOV;

// Game detection
enum class Game
{
	KOTT2,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::KOTT2, {"Knights of the Temple II", "KOTT2.exe"}},
};

const GameInfo* game = nullptr;
Game eGameType = Game::Unknown;

void Logging()
{
	// Get path to DLL
	WCHAR dllPath[_MAX_PATH] = { 0 };
	GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
	sFixPath = dllPath;
	sFixPath = sFixPath.remove_filename();

	// Get game name and exe path
	WCHAR exePathW[_MAX_PATH] = { 0 };
	GetModuleFileNameW(dllModule, exePathW, MAX_PATH);
	sExePath = exePathW;
	sExeName = sExePath.filename().string();
	sExePath = sExePath.remove_filename();

	// Spdlog initialization
	try
	{
		logger = spdlog::basic_logger_st(sFixName.c_str(), sExePath.string() + "\\" + sLogFile, true);
		spdlog::set_default_logger(logger);
		spdlog::flush_on(spdlog::level::debug);
		spdlog::set_level(spdlog::level::debug); // Enable debug level logging

		spdlog::info("----------");
		spdlog::info("{:s} v{:s} loaded.", sFixName.c_str(), sFixVersion.c_str());
		spdlog::info("----------");
		spdlog::info("Log file: {}", sExePath.string() + "\\" + sLogFile);
		spdlog::info("----------");
		spdlog::info("Module Name: {0:s}", sExeName.c_str());
		spdlog::info("Module Path: {0:s}", sExePath.string());
		spdlog::info("Module Address: 0x{0:X}", (uintptr_t)dllModule);
		spdlog::info("----------");
		spdlog::info("DLL has been successfully loaded.");
	}
	catch (const spdlog::spdlog_ex& ex)
	{
		AllocConsole();
		FILE* dummy;
		freopen_s(&dummy, "CONOUT$", "w", stdout);
		std::cout << "Log initialization failed: " << ex.what() << std::endl;
		FreeLibraryAndExitThread(thisModule, 1);
	}
}

void Configuration()
{
	// Inipp initialization
	std::ifstream iniFile(sFixPath.string() + "\\" + sConfigFile);
	if (!iniFile)
	{
		AllocConsole();
		FILE* dummy;
		freopen_s(&dummy, "CONOUT$", "w", stdout);
		std::cout << sFixName.c_str() << " v" << sFixVersion.c_str() << " loaded." << std::endl;
		std::cout << "ERROR: Could not locate config file." << std::endl;
		std::cout << "ERROR: Make sure " << sConfigFile.c_str() << " is located in " << sFixPath.string().c_str() << std::endl;
		spdlog::shutdown();
		FreeLibraryAndExitThread(thisModule, 1);
	}
	else
	{
		spdlog::info("Config file: {}", sFixPath.string() + "\\" + sConfigFile);
		ini.parse(iniFile);
	}

	// Parse config
	ini.strip_trailing_comments();
	spdlog::info("----------");

	// Load settings from ini
	inipp::get_value(ini.sections["WidescreenFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load resolution from ini
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);

	// If resolution not specified, use desktop resolution
	if (iCurrentResX <= 0 || iCurrentResY <= 0)
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();
		iCurrentResX = desktopDimensions.first;
		iCurrentResY = desktopDimensions.second;
		spdlog_confparse(iCurrentResX);
		spdlog_confparse(iCurrentResY);
	}

	spdlog::info("----------");
}

bool DetectGame()
{
	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			return true;
		}
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
	return false;
}

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * atanf(tanf(fCurrentFOV / 2.0f) * (fNewAspectRatio / fOldAspectRatio));
}

void WidescreenFix()
{
	if (eGameType == Game::KOTT2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewAspectRatio2 = 0.75f * (fOldAspectRatio / fNewAspectRatio);

		std::uint8_t* ResolutionList1ScanResult = Memory::PatternScan(exeModule, "3D 80 02 00 00 75 15 81 7C 24 14 E0 01 00 00 0F 85 8B 00 00 00 B8 01 00 00 00 EB 76 3D 20 03 00 00 75 11 81 7C 24 14 58 02 00 00 75 73 B8 02 00 00 00 EB 5E 3D 00 04 00 00 75 11 81 7C 24 14 00 03 00 00 75 5B B8 03 00 00 00 EB 46 3D 00 05 00 00 75 11 81 7C 24 14 00 04 00 00 75 43 B8 04 00 00 00 EB 2E 3D 40 06 00 00 75 11 81 7C 24 14 B0 04 00 00 75 2B B8 05 00 00 00 EB 16 3D 80 07 00 00 75 1D 81 7C 24 14 A0 05 00 00");
		if (ResolutionList1ScanResult)
		{
			spdlog::info("Resolution List 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList1ScanResult - (std::uint8_t*)exeModule);

			// 640x480
			Memory::Write(ResolutionList1ScanResult + 1, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 11, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionList1ScanResult + 29, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 39, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionList1ScanResult + 53, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 63, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionList1ScanResult + 77, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 87, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionList1ScanResult + 101, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 111, iCurrentResY);

			// 1920x1440
			Memory::Write(ResolutionList1ScanResult + 125, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 135, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list 1 memory address.");
			return;
		}

		std::uint8_t* ResolutionList2ScanResult = Memory::PatternScan(exeModule, "C7 86 44 C2 00 00 80 02 00 00 C7 86 5C C2 00 00 E0 01 00 00 C7 86 48 C2 00 00 20 03 00 00 C7 86 60 C2 00 00 58 02 00 00 C7 86 64 C2 00 00 00 03 00 00 C7 86 50 C2 00 00 00 05 00 00 C7 86 54 C2 00 00 40 06 00 00 C7 86 6C C2 00 00 B0 04 00 00 C7 86 58 C2 00 00 80 07 00 00 C7 86 70 C2 00 00 A0 05 00 00");
		if (ResolutionList2ScanResult)
		{
			spdlog::info("Resolution List 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList2ScanResult - (std::uint8_t*)exeModule);
			
			// 640x480
			Memory::Write(ResolutionList2ScanResult + 6, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 16, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionList2ScanResult + 26, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 36, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionList2ScanResult + 46, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 56, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionList2ScanResult + 66, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 76, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionList2ScanResult + 86, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 96, iCurrentResY);

			// 1920x1440
			Memory::Write(ResolutionList2ScanResult + 106, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 116, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list 2 memory address.");
			return;
		}

		std::uint8_t* MenuAndCutscenesAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 88 90 00 00 00 89 4E 60");
		if (MenuAndCutscenesAspectRatioInstructionScanResult)
		{
			spdlog::info("Menu & Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), MenuAndCutscenesAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid MenuAndCutscenesAspectRatioInstructionHook{};

			MenuAndCutscenesAspectRatioInstructionHook = safetyhook::create_mid(MenuAndCutscenesAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuAndCutscenesAspectRatio = *reinterpret_cast<float*>(ctx.eax + 0x90);

				fCurrentMenuAndCutscenesAspectRatio = fNewAspectRatio2;
			});
		}
		else
		{
			spdlog::error("Failed to locate menu & cutscenes aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* MenuAndCutscenesCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 90 8C 00 00 00 89 56 50");
		if (MenuAndCutscenesCameraFOVInstructionScanResult)
		{
			spdlog::info("Menu & Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), MenuAndCutscenesCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid MenuAndCutscenesCameraFOVInstructionHook{};
			
			MenuAndCutscenesCameraFOVInstructionHook = safetyhook::create_mid(MenuAndCutscenesCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuAndCutscenesCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0x8C);

				fCurrentMenuAndCutscenesCameraFOV = CalculateNewFOV(1.5707963268f);
			});
		}
		else
		{
			spdlog::error("Failed to locate menu & cutscenes camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* GameplayAspectRatioAndCameraFOVScanResult = Memory::PatternScan(exeModule, "68 00 00 40 3F 68 DB 0F C9 3F E8 E5 96 00 00");
		if (GameplayAspectRatioAndCameraFOVScanResult)
		{
			spdlog::info("Gameplay Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), GameplayAspectRatioAndCameraFOVScanResult + 1 - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV: Address is {:s}+{:x}", sExeName.c_str(), GameplayAspectRatioAndCameraFOVScanResult + 6 - (std::uint8_t*)exeModule);
			
			Memory::Write(GameplayAspectRatioAndCameraFOVScanResult + 1, fNewAspectRatio2);

			fNewGameplayCameraFOV = CalculateNewFOV(1.5707963268f) * fFOVFactor;

			Memory::Write(GameplayAspectRatioAndCameraFOVScanResult + 6, fNewGameplayCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate gameplay aspect ratio and camera FOV instructions memory address.");
			return;
		}

		std::uint8_t* BlackBarsAdjustInstructionScanResult = Memory::PatternScan(exeModule, "D9 86 10 06 00 00 D8 B6 24 06 00 00 74 06 D8 0D ?? ?? ?? ?? 8B 4C 24 24");
		if (BlackBarsAdjustInstructionScanResult)
		{
			spdlog::info("Black Bars Adjust Instruction: Address is {:s}+{:x}", sExeName.c_str(), BlackBarsAdjustInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid BlackBarsAdjustInstructionHook{};

			static std::vector<float> vComputedBlackBarsValues;
			
			BlackBarsAdjustInstructionHook = safetyhook::create_mid(BlackBarsAdjustInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Reference the current black bars value located at the memory address [ESI+0x610]
				float& fCurrentBlackBarsValue = *reinterpret_cast<float*>(ctx.esi + 0x610);

				fCurrentBlackBarsValue *= (fNewAspectRatio / fOldAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate black bars adjust instruction memory address.");
			return;
		}
	}
}

DWORD __stdcall Main(void*)
{
	Logging();
	Configuration();
	if (DetectGame())
	{
		WidescreenFix();
	}
	return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		thisModule = hModule;
		HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
		if (mainHandle)
		{
			SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
			CloseHandle(mainHandle);
		}
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
