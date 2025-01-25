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
std::string sFixName = "HelloKittyRollerRescueWidescreenFix";
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
	HKRR,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::HKRR, {"Hello Kitty: Roller Rescue", "Hello Kitty.exe"}},
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
	if (eGameType == Game::HKRR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewCameraFOV = fFOVFactor * (fOriginalCameraFOV * (fNewAspectRatio / fOldAspectRatio));

		std::uint8_t* Resolution1ScanResult = Memory::PatternScan(exeModule, "7C 24 14 80 02 00 00 75 11 81 7C 24 18 E0 01 00 00 75 07 83 7C");
		if (Resolution1ScanResult)
		{
			spdlog::info("Resolution Scan 1: Address is {:s}+{:x}", sExeName.c_str(), Resolution1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution1ScanResult + 0x3, iCurrentResX);

			Memory::Write(Resolution1ScanResult + 0xD, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan 1 memory address.");
			return;
		}

		std::uint8_t* Resolution2ScanResult = Memory::PatternScan(exeModule, "FF FF 7F 7F FF FF 7F 7F FF FF 7F 7F 01 01 00 00 80 02 00 00 E0 01 00 00 00 00 80 3F");
		if (Resolution2ScanResult)
		{
			spdlog::info("Resolution Scan 2: Address is {:s}+{:x}", sExeName.c_str(), Resolution2ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution2ScanResult + 0x10, iCurrentResX);

			Memory::Write(Resolution2ScanResult + 0x14, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan1Result = Memory::PatternScan(exeModule, "8B 15 64 A9 6A 00 68 AB AA AA 3F");
		if (AspectRatioScan1Result)
		{
			spdlog::info("Aspect Ratio Scan 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScan1Result + 0x7 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScan1Result + 0x7, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio scan 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScan2Result = Memory::PatternScan(exeModule, "8B 0D 60 A9 6A 00 68 AB AA AA 3F");
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

		std::uint8_t* AspectRatioScan3Result = Memory::PatternScan(exeModule, "8B 0D 88 F1 6B 00 68 AB AA AA 3F");
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

		std::uint8_t* CameraFOVScan1Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 50 56 C7 44 24 14 00 00 00 00");
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

		std::uint8_t* CameraFOVScan2Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 6A 01 50 51 E8 45 4E FF FF 83");
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

		std::uint8_t* CameraFOVScan3Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 50 E8 B2 A1 FB FF 50 E8 09 4F");
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