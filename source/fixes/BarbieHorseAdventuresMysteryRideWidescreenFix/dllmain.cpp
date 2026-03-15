// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
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
std::string sFixName = "BarbieHorseAdventuresMysteryRideWidescreenFix";
std::string sFixVersion = "1.3";
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
constexpr float fOriginalCameraFOV = 0.5f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	BHAMR,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1,
	Res2
};

enum AspectRatioInstructionsIndices
{
	AR1,
	AR2,
	AR3,
	AR4,
	AR5,
	AR6,
	AR7,
	AR8,
	AR9,
	AR10,
	AR11,
	AR12,
	AR13,
	AR14,
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	FOV5,
	FOV6,
	FOV7,
	FOV8,
	FOV9,
	FOV10,
	FOV11,
	FOV12,
	FOV13,
	FOV14,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BHAMR, {"Barbie Horse Adventures: Mystery Ride", "Barbie Horse.exe"}},
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
	if (eGameType == Game::BHAMR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "C0 74 4C 81 7D C0 80 02 00 00 7C 43 81 7D C4 E0 01 00 00", "81 7D EC 80 02 00 00 0F 85 99 00 00 00 81 7D F0 E0 01 00 00 0F 85 8C 00 00 00 8B 4D F8");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[Res1] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1] + 15, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res2] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res2] + 16, iCurrentResY);
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "75 00 A3 F0 A8 76 00 68 AB AA AA 3F", "8A F7 FF 83 C4 0C 68 AB AA AA 3F",
		"26 F7 FF 83 C4 0C 68 AB AA AA 3F 68", "A3 C4 31 77 00 68 AB AA AA 3F 68", "D8 E9 98 00 00 00 68 AB AA AA 3F 68", "04 A3 F0 A8 76 00 68 AB AA AA 3F 68",
		"ED FF 83 C4 04 89 45 F8 68 AB AA AA 3F 68", "15 F0 A8 76 00 89 15 C4 31 77 00 68 AB AA AA 3F", "EC FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68",
		"04 8B 0D F0 A8 76 00 89 0D C4 31 77 00 68 AB AA AA 3F 68", "D6 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68",
		"00 51 E8 58 B5 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68", "0D F0 A8 76 00 68 AB AA AA 3F", "00 52 E8 9F 5C EB FF 83 C4 04 A3 C4 31 77 00 A1 C4 31 77 00 A3 F0 A8 76 00 68 AB AA AA 3F 68");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1] + 7 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR3] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR4] + 5 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR5] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR6] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR7] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR8] + 11 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR9] + 22 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR10] + 13 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 11: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR11] + 23 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 12: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR12] + 27 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 13: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR13] + 5 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 14: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR14] + 25 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[AR1] + 8, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR3] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR4] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR5] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR6] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR7] + 9, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR8] + 12, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR9] + 23, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR10] + 14, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR11] + 24, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR12] + 28, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR13] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR14] + 26, fNewAspectRatio);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D 48", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 B5",
		"68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 7E", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 CE", "68 00 00 00 3F 8B 45",
		"68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 8A", "68 00 00 00 3F A1 C8 31 77 00 50 8B 4D", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D C4",
		"68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 2A", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 97", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 0E",
		"68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 D2", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 6B", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 1A");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV7] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV8] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV9] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV10] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 11: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV11] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 12: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV12] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 13: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV13] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 14: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV14] - (std::uint8_t*)exeModule);

			fNewCameraFOV = fOriginalCameraFOV * fAspectRatioScale * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[FOV1] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV5] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV6] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV7] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV8] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV9] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV10] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV11] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV12] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV13] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV14] + 1, fNewCameraFOV);
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