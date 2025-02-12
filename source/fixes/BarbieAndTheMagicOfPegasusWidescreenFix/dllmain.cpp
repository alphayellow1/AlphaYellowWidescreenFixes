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
std::string sFixName = "BarbieAndTheMagicOfPegasusWidescreenFix";
std::string sFixVersion = "1.1";
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
	BATMOP,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BATMOP, {"Barbie and the Magic of Pegasus", "Barbie Pegasus.exe"}},
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
	if (eGameType == Game::BATMOP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewCameraFOV = fFOVFactor * (fOriginalCameraFOV * (fNewAspectRatio / fOldAspectRatio));

		std::uint8_t* ResolutionWidthScanResult = Memory::PatternScan(exeModule, "3C 81 FD 80 02 00 00 7C 46 8B");
		if (ResolutionWidthScanResult)
		{
			spdlog::info("Resolution Width: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthScanResult + 0x3 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionWidthScanResult + 0x3, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution width memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightScanResult = Memory::PatternScan(exeModule, "40 81 F9 E0 01 00 00 7C 3A 8B");
		if (ResolutionHeightScanResult)
		{
			spdlog::info("Resolution Height: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightScanResult + 0x3 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionHeightScanResult + 0x3, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution width memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan1Result = Memory::PatternScan(exeModule, "68 81 89 00 68 AB AA AA 3F 68");
		if (AspectRatioScan1Result)
		{
			spdlog::info("Aspect Ratio Scan 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan1Result + 0x5 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan1Result + 0x5, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan2Result = Memory::PatternScan(exeModule, "24 08 8B 15 D4 8A 77 00 68 AB AA AA 3F 68");
		if (AspectRatioScan2Result)
		{
			spdlog::info("Aspect Ratio Scan 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan2Result + 0x9 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan2Result + 0x9, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan3Result = Memory::PatternScan(exeModule, "FC FF 68 AB AA AA 3F 8B F0 A1");
		if (AspectRatioScan3Result)
		{
			spdlog::info("Aspect Ratio Scan 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan3Result + 0x9 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan3Result + 0x3, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 3 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan4Result = Memory::PatternScan(exeModule, "B9 08 00 8B 0D 68 81 89 00 A1 D4 8A 77 00 68 AB AA AA 3F 68");
		if (AspectRatioScan4Result)
		{
			spdlog::info("Aspect Ratio Scan 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan4Result + 0xF - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan4Result + 0xF, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 4 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan5Result = Memory::PatternScan(exeModule, "99 08 00 8B 0D 68 81 89 00 A1 D4 8A 77 00 68 AB AA AA 3F 68");
		if (AspectRatioScan5Result)
		{
			spdlog::info("Aspect Ratio Scan 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan5Result + 0xF - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan5Result + 0xF, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 5 memory address.");
			return;
		}

		std::uint8_t* CameraFOVScan1Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 51 50 A3 D4");
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

		std::uint8_t* CameraFOVScan2Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 51 52 E8");
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

		std::uint8_t* CameraFOVScan3Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 50 56");
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

		std::uint8_t* CameraFOVScan4Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 51 50 A3 64 81 89 00 E8 1C");
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

		std::uint8_t* CameraFOVScan5Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 51 50 A3 64 81 89 00 E8 41");
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
