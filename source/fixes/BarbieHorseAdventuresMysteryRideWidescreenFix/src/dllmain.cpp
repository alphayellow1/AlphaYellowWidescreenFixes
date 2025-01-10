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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float fOriginalCameraFOV = 0.5f;

// Ini variables
bool bFixActive;

// Variables
uint32_t iCurrentResX;
uint32_t iCurrentResY;
float fNewAspectRatio;
float fNewCameraFOV;
float fFOVFactor;

// Game detection
enum class Game
{
	BHAMR,
	Unknown
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

void WidescreenFix()
{
	if (eGameType == Game::BHAMR && bFixActive == true) {
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewCameraFOV = fFOVFactor * (fOriginalCameraFOV * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio));

		std::uint8_t* ResolutionWidthScanResult = Memory::PatternScan(exeModule, "C0 80 02 00 00 7C");
		if (ResolutionWidthScanResult)
		{
			spdlog::info("Resolution Width: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthScanResult + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionWidthScanResult + 0x1, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution width memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightScanResult = Memory::PatternScan(exeModule, "C4 E0 01 00 00 7C");
		if (ResolutionHeightScanResult)
		{
			spdlog::info("Resolution Height: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightScanResult + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionHeightScanResult + 0x1, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution width memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan1Result = Memory::PatternScan(exeModule, "75 00 A3 F0 A8 76 00 68 AB AA AA 3F");
		if (AspectRatioScan1Result)
		{
			spdlog::info("Aspect Ratio Scan 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan1Result + 0x8 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan1Result + 0x8, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan2Result = Memory::PatternScan(exeModule, "8A F7 FF 83 C4 0C 68 AB AA AA 3F");
		if (AspectRatioScan2Result)
		{
			spdlog::info("Aspect Ratio Scan 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan2Result + 0x7 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan2Result + 0x7, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan3Result = Memory::PatternScan(exeModule, "26 F7 FF 83 C4 0C 68 AB AA AA 3F 68");
		if (AspectRatioScan3Result)
		{
			spdlog::info("Aspect Ratio Scan 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan3Result + 0x7 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan3Result + 0x7, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 3 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan4Result = Memory::PatternScan(exeModule, "A3 C4 31 77 00 68 AB AA AA 3F 68");
		if (AspectRatioScan4Result)
		{
			spdlog::info("Aspect Ratio Scan 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan4Result + 0x6 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan4Result + 0x6, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 4 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan5Result = Memory::PatternScan(exeModule, "D8 E9 98 00 00 00 68 AB AA AA 3F 68");
		if (AspectRatioScan5Result)
		{
			spdlog::info("Aspect Ratio Scan 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan5Result + 0x7 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan5Result + 0x7, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 5 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan6Result = Memory::PatternScan(exeModule, "04 A3 F0 A8 76 00 68 AB AA AA 3F 68");
		if (AspectRatioScan6Result)
		{
			spdlog::info("Aspect Ratio Scan 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan6Result + 0x7 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan6Result + 0x7, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 6 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan7Result = Memory::PatternScan(exeModule, "ED FF 83 C4 04 89 45 F8 68 AB AA AA 3F 68");
		if (AspectRatioScan7Result)
		{
			spdlog::info("Aspect Ratio Scan 7: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan7Result + 0x9 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan7Result + 0x9, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 7 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan8Result = Memory::PatternScan(exeModule, "15 F0 A8 76 00 89 15 C4 31 77 00 68 AB AA AA 3F");
		if (AspectRatioScan8Result)
		{
			spdlog::info("Aspect Ratio Scan 8: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan8Result + 0xC - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan8Result + 0xC, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 8 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan9Result = Memory::PatternScan(exeModule, "EC FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68");
		if (AspectRatioScan9Result)
		{
			spdlog::info("Aspect Ratio Scan 9: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan9Result + 0x17 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan9Result + 0x17, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 9 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan10Result = Memory::PatternScan(exeModule, "04 8B 0D F0 A8 76 00 89 0D C4 31 77 00 68 AB AA AA 3F 68");
		if (AspectRatioScan10Result)
		{
			spdlog::info("Aspect Ratio Scan 10: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan10Result + 0xE - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan10Result + 0xE, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 10 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan11Result = Memory::PatternScan(exeModule, "D6 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68");
		if (AspectRatioScan11Result)
		{
			spdlog::info("Aspect Ratio Scan 11: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan11Result + 0x18 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan11Result + 0x18, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 11 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan12Result = Memory::PatternScan(exeModule, "00 51 E8 58 B5 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68");
		if (AspectRatioScan12Result)
		{
			spdlog::info("Aspect Ratio Scan 12: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan12Result + 0x1C - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan12Result + 0x1C, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 12 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan13Result = Memory::PatternScan(exeModule, "0D F0 A8 76 00 68 AB AA AA 3F");
		if (AspectRatioScan13Result)
		{
			spdlog::info("Aspect Ratio Scan 13: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan13Result + 0x6 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan13Result + 0x6, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 13 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan14Result = Memory::PatternScan(exeModule, "00 52 E8 9F 5C EB FF 83 C4 04 A3 C4 31 77 00 A1 C4 31 77 00 A3 F0 A8 76 00 68 AB AA AA 3F 68");
		if (AspectRatioScan14Result)
		{
			spdlog::info("Aspect Ratio Scan 14: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan14Result + 0x1A - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan14Result + 0x1A, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 14 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan1Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D 48");
		if (CameraFOVScan1Result)
		{
			spdlog::info("Camera FOV Scan 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan1Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan1Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan2Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 B5");
		if (CameraFOVScan2Result)
		{
			spdlog::info("Camera FOV Scan 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan2Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan2Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan3Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 7E");
		if (CameraFOVScan3Result)
		{
			spdlog::info("Camera FOV Scan 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan3Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan3Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan4Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 CE");
		if (CameraFOVScan4Result)
		{
			spdlog::info("Camera FOV Scan 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan4Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan4Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 4 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan5Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 45");
		if (CameraFOVScan5Result)
		{
			spdlog::info("Camera FOV Scan 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan5Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan5Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 5 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan6Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 8A");
		if (CameraFOVScan6Result)
		{
			spdlog::info("Camera FOV Scan 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan6Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan6Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 6 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan7Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 4D");
		if (CameraFOVScan7Result)
		{
			spdlog::info("Camera FOV Scan 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan7Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan7Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 7 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan8Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D C4");
		if (CameraFOVScan8Result)
		{
			spdlog::info("Camera FOV Scan 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan8Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan8Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 8 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan9Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 2A");
		if (CameraFOVScan9Result)
		{
			spdlog::info("Camera FOV Scan 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan9Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan9Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 9 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan10Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 97");
		if (CameraFOVScan10Result)
		{
			spdlog::info("Camera FOV Scan 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan10Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan10Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 10 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan11Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 0E");
		if (CameraFOVScan11Result)
		{
			spdlog::info("Camera FOV Scan 11: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan11Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan11Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 11 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan12Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 D2");
		if (CameraFOVScan12Result)
		{
			spdlog::info("Camera FOV Scan 12: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan12Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan12Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 12 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan13Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 6B");
		if (CameraFOVScan13Result)
		{
			spdlog::info("Camera FOV Scan 13: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan13Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan13Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 13 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan14Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 1A");
		if (CameraFOVScan14Result)
		{
			spdlog::info("Camera FOV Scan 14: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScan14Result + 0x1 - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVScan14Result + 0x1, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan 14 memory address.");
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