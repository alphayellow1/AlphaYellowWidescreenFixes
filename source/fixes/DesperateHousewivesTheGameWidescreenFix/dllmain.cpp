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
#include <cmath> // For atanf, tanf
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <vector>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "DesperateHousewivesTheGameWidescreenFix";
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
constexpr float fTolerance = 0.000001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraFOV;
float fAspectRatioScale;

// Game detection
enum class Game
{
	DHTG,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DHTG, {"Desperate Housewives: The Game", "DesperateHousewives.exe"}},
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
	GetModuleFileNameW(exeModule, exePathW, MAX_PATH);
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
		spdlog::info("Module Address: 0x{0:X}", (uintptr_t)exeModule);
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

static SafetyHookMid FamilySelectionAspectRatioInstructionHook{};

void FamilySelectionAspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewAspectRatio]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::DHTG && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
		
		std::uint8_t* ResolutionList1ScanResult = Memory::PatternScan(exeModule, "C7 00 80 02 00 00 C7 01 E0 01 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 20 03 00 00 C7 00 58 02 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 00 04 00 00 C7 02 00 03 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 80 04 00 00 C7 01 60 03 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 05 00 00 C7 00 C0 03 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 00 05 00 00 C7 02 00 04 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 40 06 00 00 C7 01 B0 04 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 04 00 00 C7 00 00 03 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 C0 03 00 00 C7 02 D0 02 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 B0 04 00 00 C7 01 84 03 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 2B 04 00 00 C7 00 20 03 00 00");
		if (ResolutionList1ScanResult)
		{
			spdlog::info("Resolution List 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionList1ScanResult + 2, iCurrentResX); // 640

			Memory::Write(ResolutionList1ScanResult + 8, iCurrentResY); // 480

			Memory::Write(ResolutionList1ScanResult + 23, iCurrentResX); // 800

			Memory::Write(ResolutionList1ScanResult + 29, iCurrentResY); // 600

			Memory::Write(ResolutionList1ScanResult + 44, iCurrentResX); // 1024

			Memory::Write(ResolutionList1ScanResult + 50, iCurrentResY); // 768

			Memory::Write(ResolutionList1ScanResult + 65, iCurrentResX); // 1152

			Memory::Write(ResolutionList1ScanResult + 71, iCurrentResY); // 864

			Memory::Write(ResolutionList1ScanResult + 86, iCurrentResX); // 1290

			Memory::Write(ResolutionList1ScanResult + 92, iCurrentResY); // 960
			
			Memory::Write(ResolutionList1ScanResult + 107, iCurrentResX); // 1280

			Memory::Write(ResolutionList1ScanResult + 113, iCurrentResY); // 1024

			Memory::Write(ResolutionList1ScanResult + 128, iCurrentResX); // 1600

			Memory::Write(ResolutionList1ScanResult + 134, iCurrentResY); // 1200

			Memory::Write(ResolutionList1ScanResult + 149, iCurrentResX); // 1024

			Memory::Write(ResolutionList1ScanResult + 155, iCurrentResY); // 768

			Memory::Write(ResolutionList1ScanResult + 170, iCurrentResX); // 960

			Memory::Write(ResolutionList1ScanResult + 176, iCurrentResY); // 720

			Memory::Write(ResolutionList1ScanResult + 191, iCurrentResX); // 1200

			Memory::Write(ResolutionList1ScanResult + 197, iCurrentResY); // 900

			Memory::Write(ResolutionList1ScanResult + 212, iCurrentResX); // 1067

			Memory::Write(ResolutionList1ScanResult + 218, iCurrentResY); // 800
		}
		else
		{
			spdlog::error("Failed to locate resolution list 1 memory address.");
			return;
		}
	
	    std::uint8_t* ResolutionList2ScanResult = Memory::PatternScan(exeModule, "C7 00 80 02 00 00 C7 01 E0 01 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 20 03 00 00 C7 00 58 02 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 00 04 00 00 C7 02 00 03 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 80 04 00 00 C7 01 60 03 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 05 00 00 C7 00 C0 03 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 00 05 00 00 C7 02 00 04 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 40 06 00 00 C7 01 B0 04 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 05 00 00 C7 00 00 03 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 00 05 00 00 C7 02 D0 02 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 40 06 00 00 C7 01 84 03 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 05 00 00 C7 00 20 03 00 00");
		if (ResolutionList2ScanResult)
		{
			spdlog::info("Resolution List 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(ResolutionList2ScanResult + 2, iCurrentResX); // 640

			Memory::Write(ResolutionList2ScanResult + 8, iCurrentResY); // 480

			Memory::Write(ResolutionList2ScanResult + 23, iCurrentResX); // 800

			Memory::Write(ResolutionList2ScanResult + 29, iCurrentResY); // 600

			Memory::Write(ResolutionList2ScanResult + 44, iCurrentResX); // 1024

			Memory::Write(ResolutionList2ScanResult + 50, iCurrentResY); // 768

			Memory::Write(ResolutionList2ScanResult + 65, iCurrentResX); // 1152

			Memory::Write(ResolutionList2ScanResult + 71, iCurrentResY); // 864

			Memory::Write(ResolutionList2ScanResult + 86, iCurrentResX); // 1290

			Memory::Write(ResolutionList2ScanResult + 92, iCurrentResY); // 960

			Memory::Write(ResolutionList2ScanResult + 107, iCurrentResX); // 1280

			Memory::Write(ResolutionList2ScanResult + 113, iCurrentResY); // 1024

			Memory::Write(ResolutionList2ScanResult + 128, iCurrentResX); // 1600

			Memory::Write(ResolutionList2ScanResult + 134, iCurrentResY); // 1200

			Memory::Write(ResolutionList2ScanResult + 149, iCurrentResX); // 1280

			Memory::Write(ResolutionList2ScanResult + 155, iCurrentResY); // 768

			Memory::Write(ResolutionList2ScanResult + 170, iCurrentResX); // 1280

			Memory::Write(ResolutionList2ScanResult + 176, iCurrentResY); // 720

			Memory::Write(ResolutionList2ScanResult + 191, iCurrentResX); // 1600

			Memory::Write(ResolutionList2ScanResult + 197, iCurrentResY); // 900

			Memory::Write(ResolutionList2ScanResult + 212, iCurrentResX); // 1280

			Memory::Write(ResolutionList2ScanResult + 218, iCurrentResY); // 800
		}
		else
		{
			spdlog::error("Failed to locate resolution list 2 memory address.");
			return;
		}

		std::uint8_t* FamilySelectionAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 58 04 D9 E8 D9 58 08");
		if (FamilySelectionAspectRatioInstructionScanResult)
		{
			spdlog::info("Family Selection Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), FamilySelectionAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(FamilySelectionAspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			FamilySelectionAspectRatioInstructionHook = safetyhook::create_mid(FamilySelectionAspectRatioInstructionScanResult + 6, FamilySelectionAspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate family selection aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 03 D9 1C 24 50 E8 ?? ?? ?? ?? 8D 4C 24 54");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid CameraFOVInstructionMidHook{};

			static std::vector<float> vComputedFOVs;

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ebx);

				// Computes the new FOV value
				if (fCurrentCameraFOV == 0.7853981256f)
				{
					fCurrentCameraFOV *= fFOVFactor;
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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