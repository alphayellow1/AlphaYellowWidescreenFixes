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
std::string sFixName = "SnowboardParkTycoonWidescreenFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;

// Game detection
enum class Game
{
	SPT,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SPT, {"Snowboard Park Tycoon", "SnowBoard_Core.exe"}},
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
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}
}

void WidescreenFix()
{
	if (eGameType == Game::SPT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		/*
		std::uint8_t* ResolutionScanResult = Memory::PatternScan(exeModule, "C7 05 B8 98 61 00 00 04 00 00 C7 05 BC 98 61 00 00 03 00 00 C2 04 00 C7 05 B8 98 61 00 20 03 00 00 C7 05 BC 98 61 00 58 02 00 00 C2 04 00 C7 05 B8 98 61 00 80 02 00 00 C7 05 BC 98 61 00 E0 01 00 00 C2 04 00");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScanResult + 0x6, iCurrentResX);

			Memory::Write(ResolutionScanResult + 0x10, iCurrentResY);

			Memory::Write(ResolutionScanResult + 0x1D, iCurrentResX);

			Memory::Write(ResolutionScanResult + 0x27, iCurrentResY);

			Memory::Write(ResolutionScanResult + 0x34, iCurrentResX);

			Memory::Write(ResolutionScanResult + 0x3E, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan memory address.");
			return;
		}
		*/

		std::uint8_t* GroundCameraAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 4E 3C D9 5A 14 D9 52 28 D8 4E 30");
		if (GroundCameraAspectRatioInstructionScanResult)
		{
			spdlog::info("Ground Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), GroundCameraAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid GroundCameraAspectRatioInstructionMidHook{};

			GroundCameraAspectRatioInstructionMidHook = safetyhook::create_mid(GroundCameraAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esi + 0x3C) = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate ground camera aspect ratio instruction memory address.");
			return;
		}

		/*
		std::uint8_t* GroundCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 46 58 F3 AB 5F D8 3D ?? ?? ?? ??");
		if (GroundCameraFOVInstructionScanResult)
		{
			spdlog::info("Ground Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GroundCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid GroundCameraFOVInstructionMidHook{};

			static float lastModifiedFOV1 = 0.0f;

			GroundCameraFOVInstructionMidHook = safetyhook::create_mid(GroundCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& currentFOVValue1 = *reinterpret_cast<float*>(ctx.esi + 0x58);

				if (currentFOVValue1 != lastModifiedFOV1)
				{
					float modifiedFOVValue1 = fFOVFactor * (currentFOVValue1 / (fOldAspectRatio / fNewAspectRatio));

					if (currentFOVValue1 != modifiedFOVValue1)
					{
						currentFOVValue1 = modifiedFOVValue1;
						lastModifiedFOV1 = modifiedFOVValue1;
					}
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate ground camera FOV instruction memory address.");
			return;
		}
		*/

		std::uint8_t* OverviewCameraAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 4E 3C 8B 4C 24 20 89 4C 24 14");
		if (OverviewCameraAspectRatioInstructionScanResult)
		{
			spdlog::info("Overview Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid OverviewCameraAspectRatioInstructionMidHook{};

			OverviewCameraAspectRatioInstructionMidHook = safetyhook::create_mid(OverviewCameraAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esi + 0x3C) = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 49 44 D8 64 24 00 D8 71 3C D9 41 50");
		if (OverviewCameraFOVInstructionScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid OverviewCameraFOVInstructionMidHook{};

			static float lastModifiedFOV2 = 0.0f;

			OverviewCameraFOVInstructionMidHook = safetyhook::create_mid(OverviewCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& currentFOVValue2 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

				if (currentFOVValue2 != lastModifiedFOV2)
				{
					float modifiedFOVValue2 = fFOVFactor * (currentFOVValue2 / (fOldAspectRatio / fNewAspectRatio));

					if (currentFOVValue2 != modifiedFOVValue2)
					{
						currentFOVValue2 = modifiedFOVValue2;
						lastModifiedFOV2 = modifiedFOVValue2;
					}
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction memory address.");
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