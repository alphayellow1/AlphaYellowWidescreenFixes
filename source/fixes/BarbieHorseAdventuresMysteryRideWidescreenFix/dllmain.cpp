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
std::string sFixVersion = "1.2";
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

enum AspectRatioInstructionsIndex
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan,
	AspectRatio4Scan,
	AspectRatio5Scan,
	AspectRatio6Scan,
	AspectRatio7Scan,
	AspectRatio8Scan,
	AspectRatio9Scan,
	AspectRatio10Scan,
	AspectRatio11Scan,
	AspectRatio12Scan,
	AspectRatio13Scan,
	AspectRatio14Scan,
};

enum CameraFOVInstructionsIndex
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan,
	CameraFOV4Scan,
	CameraFOV5Scan,
	CameraFOV6Scan,
	CameraFOV7Scan,
	CameraFOV8Scan,
	CameraFOV9Scan,
	CameraFOV10Scan,
	CameraFOV11Scan,
	CameraFOV12Scan,
	CameraFOV13Scan,
	CameraFOV14Scan,
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

		fNewCameraFOV = fFOVFactor * Maths::CalculateNewFOV_MultiplierBased(fOriginalCameraFOV, fAspectRatioScale);

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "C0 74 4C 81 7D C0 80 02 00 00 7C 43 81 7D C4 E0 01 00 00");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScanResult + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScanResult + 15, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution width memory address.");
			return;
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "75 00 A3 F0 A8 76 00 68 AB AA AA 3F", "8A F7 FF 83 C4 0C 68 AB AA AA 3F", "26 F7 FF 83 C4 0C 68 AB AA AA 3F 68", "A3 C4 31 77 00 68 AB AA AA 3F 68", "D8 E9 98 00 00 00 68 AB AA AA 3F 68", "04 A3 F0 A8 76 00 68 AB AA AA 3F 68", "ED FF 83 C4 04 89 45 F8 68 AB AA AA 3F 68", "15 F0 A8 76 00 89 15 C4 31 77 00 68 AB AA AA 3F", "EC FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68", "04 8B 0D F0 A8 76 00 89 0D C4 31 77 00 68 AB AA AA 3F 68", "D6 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68", "00 51 E8 58 B5 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68", "0D F0 A8 76 00 68 AB AA AA 3F", "00 52 E8 9F 5C EB FF 83 C4 04 A3 C4 31 77 00 A1 C4 31 77 00 A3 F0 A8 76 00 68 AB AA AA 3F 68");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instructions Scan 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 6: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 7: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 8: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 9: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio9Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 10: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio10Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 11: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio11Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 12: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio12Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 13: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio13Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instructions Scan 14: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio14Scan] - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio1Scan] + 8, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio2Scan] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio3Scan] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio4Scan] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio5Scan] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio6Scan] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio7Scan] + 9, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio8Scan] + 12, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio9Scan] + 23, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio10Scan] + 14, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio11Scan] + 24, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio12Scan] + 28, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio13Scan] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio14Scan] + 26, fNewAspectRatio);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D 48", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 B5", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 7E", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 CE", "68 00 00 00 3F 8B 45", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 8A", "68 00 00 00 3F A1 C8 31 77 00 50 8B 4D", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D C4", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 2A", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 97", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 0E", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 D2", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 6B", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 1A");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instructions Scan 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV9Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV10Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 11: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV11Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 12: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV12Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 13: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV13Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 14: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV14Scan] - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV1Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV2Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV3Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV4Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV5Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV6Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV7Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV8Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV9Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV10Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV11Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV12Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV13Scan] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV14Scan] + 1, fNewCameraFOV);
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