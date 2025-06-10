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
HMODULE thisModule;

// Fix details
std::string sFixName = "RolandGarrosFrenchOpen2001WidescreenFix";
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

// Ini variables
bool bFixActive;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewAspectRatio;
float fFOVFactor;
static float fCurrentCameraFOV;

// Game detection
enum class Game
{
	RGFO2001,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RGFO2001, {"Roland Garros French Open 2001", "RG2001.exe"}},
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

void WidescreenFix()
{
	if (eGameType == Game::RGFO2001 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "66 C7 41 58 90 01 66 C7 41 5A 2C 01 C2 04 00 66 C7 41 58 00 02 66 C7 41 5A 80 01 C2 04 00 66 C7 41 58 80 02 66 C7 41 5A 90 01 C2 04 00 66 C7 41 58 80 02 66 C7 41 5A E0 01 C2 04 00 66 C7 41 58 20 03 66 C7 41 5A 58 02 C2 04 00 66 C7 41 58 00 04 66 C7 41 5A 00 03 C2 04 00 66 C7 41 58 00 05 66 C7 41 5A 00 04 C2 04 00 66 C7 41 58 40 06 66 C7 41 5A B0 04");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionListScanResult + 4, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 10, (uint16_t)iCurrentResY);

			Memory::Write(ResolutionListScanResult + 19, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 25, (uint16_t)iCurrentResY);

			Memory::Write(ResolutionListScanResult + 34, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 40, (uint16_t)iCurrentResY);

			Memory::Write(ResolutionListScanResult + 49, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 55, (uint16_t)iCurrentResY);

			Memory::Write(ResolutionListScanResult + 64, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 70, (uint16_t)iCurrentResY);

			Memory::Write(ResolutionListScanResult + 79, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 85, (uint16_t)iCurrentResY);

			Memory::Write(ResolutionListScanResult + 94, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 100, (uint16_t)iCurrentResY);

			Memory::Write(ResolutionListScanResult + 109, (uint16_t)iCurrentResX);

			Memory::Write(ResolutionListScanResult + 115, (uint16_t)iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 B4 81 FC 00 00 00 D9 5D FC 8D 45 F8 50 8B 45 08 8B 4D F4 FF 74 81 68");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid AspectRatioInstructionMidHook{};

			AspectRatioInstructionMidHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fAspectRatio = *reinterpret_cast<float*>(ctx.ecx + ctx.eax * 0x4 + 0xFC);

				fAspectRatio = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 84 81 AC 00 00 00 D8 8C 96 C4 00 00 00 D9 5D F8 8B 45 08");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ecx + ctx.eax * 0x4 + 0xAC);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOV2InstructionScanResult = Memory::PatternScan(exeModule, "D8 8C 96 C4 00 00 00 8B 45 08 8B 4D F4");
		if (CameraFOV2InstructionScanResult)
		{
			spdlog::info("Camera FOV 2 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV2InstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraFOV2InstructionMidHook{};

			CameraFOV2InstructionMidHook = safetyhook::create_mid(CameraFOV2InstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esi + ctx.edx * 0x4 + 0xC4);

				if (fCurrentCameraFOV == 0.4149999917f)
				{
					fCurrentCameraFOV2 = (fNewAspectRatio / fOldAspectRatio) * fFOVFactor;
				}
				else
				{
					fCurrentCameraFOV2 = fNewAspectRatio / fOldAspectRatio;
				}

			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV 2 instruction memory address.");
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