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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "DreamPinball3DWidescreenFix";
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
constexpr float fOriginalCameraFOV = 1.04719758033752f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraFOV;

// Game detection
enum class Game
{
	DP3D,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DP3D, {"Dream Pinball 3D", "dp3d.exe"}},
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
	if (bFixActive == true && eGameType == Game::DP3D)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* MainMenuResolution1ScanResult = Memory::PatternScan(exeModule, "68 58 02 00 00 68 20 03 00 00 C7 05 48 7D 4A 00 20 03 00 00 C7 05 4C 7D 4A 00 58 02 00 00 E8 AC 7B FB FF 83 C4 08 C7 05 88 33 2C 01");
		if (MainMenuResolution1ScanResult)
		{
			spdlog::info("Main Menu Resolution 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), MainMenuResolution1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(MainMenuResolution1ScanResult + 6, iCurrentResX);

			Memory::Write(MainMenuResolution1ScanResult + 1, iCurrentResY);

			Memory::Write(MainMenuResolution1ScanResult + 16, iCurrentResX);

			Memory::Write(MainMenuResolution1ScanResult + 26, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate main menu resolution 1 scan memory address.");
			return;
		}

		std::uint8_t* MainMenuResolution2ScanResult = Memory::PatternScan(exeModule, "68 58 02 00 00 83 F2 01 0F 95 C0 68 20 03 00 00 68 58 02 00 00 68 20 03 00 00 C7 05 48 7D 4A 00 20 03 00 00 C7 05 4C 7D 4A 00 58 02 00 00");
		if (MainMenuResolution2ScanResult)
		{
			spdlog::info("Main Menu Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), MainMenuResolution2ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(MainMenuResolution2ScanResult + 12, iCurrentResX);

			Memory::Write(MainMenuResolution2ScanResult + 1, iCurrentResY);

			Memory::Write(MainMenuResolution2ScanResult + 22, iCurrentResX);

			Memory::Write(MainMenuResolution2ScanResult + 17, iCurrentResY);

			Memory::Write(MainMenuResolution2ScanResult + 32, iCurrentResX);

			Memory::Write(MainMenuResolution2ScanResult + 42, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate main menu resolution 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 05 2C 24 5C 00 80 02 00 00 C7 05 28 24 5C 00 E0 01 00 00 89 15 20 24 5C 00 89 0D 24 24 5C 00 75 21 C7 05 2C 24 5C 00 80 02 00 00 C7 05 28 24 5C 00 E0 01 00 00 89 15 20 24 5C 00 89 0D 24 24 5C 00 C3 83 F8 01 75 21 C7 05 2C 24 5C 00 20 03 00 00 C7 05 28 24 5C 00 58 02 00 00 89 15 20 24 5C 00 89 0D 24 24 5C 00 C3 83 F8 02 75 21 C7 05 2C 24 5C 00 00 04 00 00 C7 05 28 24 5C 00 00 03 00 00 89 15 20 24 5C 00 89 0D 24 24 5C 00 C3 83 F8 03 75 21 C7 05 2C 24 5C 00 00 05 00 00 C7 05 28 24 5C 00 C0 03 00 00 89 15 20 24 5C 00 89 0D 24 24 5C 00 C3 83 F8 04 75 21 C7 05 2C 24 5C 00 00 05 00 00 C7 05 28 24 5C 00 00 04 00 00 89 15 20 24 5C 00 89 0D 24 24 5C 00 C3 83 F8 05 75 20 C7 05 2C 24 5C 00 40 06 00 00 C7 05 28 24 5C 00 B0 04 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionListScanResult + 6, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 40, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 50, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 78, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 88, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 116, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 126, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 154, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 164, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 192, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 202, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 230, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 240, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}			

		std::uint8_t* CameraFOVandAspectRatioScanResult = Memory::PatternScan(exeModule, "92 0A 86 3F 39 8E E3 3F 4F 6E 52 65 73 65 74 44 65 76 69 63 65");
		if (CameraFOVandAspectRatioScanResult)
		{
			spdlog::info("Camera FOV and Aspect Ratio Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVandAspectRatioScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV = fOriginalCameraFOV * fFOVFactor;

			Memory::Write(CameraFOVandAspectRatioScanResult, fNewCameraFOV);

			Memory::Write(CameraFOVandAspectRatioScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV and aspect ratio scan memory address.");
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