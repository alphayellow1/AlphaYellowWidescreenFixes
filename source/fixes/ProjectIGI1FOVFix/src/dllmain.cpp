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
#include <cmath> // For atan, tan
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "ProjectIGI1FOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float fOriginalWeaponFOV = 1.75589966773987f;

// Ini variables
bool FixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fCameraFOVFactor;
float fWeaponFOVFactor;
float fNewCameraFOV;
float fNewAspectRatio;
float fNewWeaponFOV;
uint16_t GameVersionCheckValue;

// Game detection
enum class Game
{
	PI1,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PI1, {"Project IGI 1: I'm Going In", "IGI.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixActive);
	spdlog_confparse(FixActive);

	// Load resolution from ini
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", fCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", fWeaponFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fCameraFOVFactor);
	spdlog_confparse(fWeaponFOVFactor);

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

uint16_t GameVersionCheck()
{
	std::uint8_t* CheckValueResultScan = Memory::PatternScan(exeModule, "4C 01 04 00 ?? ??");

	uint16_t CheckValue = *reinterpret_cast<uint16_t*>(CheckValueResultScan + 0x4);

	switch (CheckValue)
	{
	case 29894:
		spdlog::info("Chinese / European version detected.");
		break;

	case 19290:
		spdlog::info("American version detected.");
		break;

	case 1571:
		spdlog::info("Japanese version detected.");
		break;

	default:
		spdlog::info("Unknown version detected. Please make sure to have either the Chinese / European, Japanese or American version present.");
		return 0;
	}

	return CheckValue;
}

void FOVFix()
{
	if (eGameType == Game::PI1 && FixActive == true) {
		GameVersionCheckValue = GameVersionCheck();

		std::uint8_t* CameraFOVScanResult = Memory::PatternScan(exeModule, "C7 44 24 0C 00 00 00 00 56 57 68 00 00 80 3F 6A 00 6A 00 6A 00 6A");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScanResult + 0xB - (std::uint8_t*)exeModule);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV memory address.");
			return;
		}

		fNewCameraFOV = (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) * 0.75f;

		Memory::Write(CameraFOVScanResult + 0xB, fNewCameraFOV * fCameraFOVFactor);

		fNewAspectRatio = (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) * 0.75f * 4.0f;

		if (GameVersionCheckValue == 29894) {
			std::uint8_t* AspectRatioScanResult = Memory::PatternScan(exeModule, "00 00 E0 3F 00 00 80 40 33 33 B3 3F");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScanResult + 0x4 - (std::uint8_t*)exeModule);
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio memory address.");
				return;
			}

			Memory::Write(AspectRatioScanResult + 0x4, fNewAspectRatio);
		}
		else if (GameVersionCheckValue == 19290 || GameVersionCheckValue == 1571) {
			std::uint8_t* AspectRatioScanResult = Memory::PatternScan(exeModule, "00 00 E0 3F 00 00 00 40 00 00 80 40 CD CC 4C 3D");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScanResult + 0x8 - (std::uint8_t*)exeModule);
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio memory address.");
				return;
			}

			Memory::Write(AspectRatioScanResult + 0x8, fNewAspectRatio);
		}

		std::uint8_t* WeaponFOVScanResult = Memory::PatternScan(exeModule, "9A 99 19 44 00 00 00 40 52 C1 E0 3F 56 55 85 3F");
		if (WeaponFOVScanResult)
		{
			spdlog::info("Weapon FOV: Address is {:s}+{:x}", sExeName.c_str(), WeaponFOVScanResult + 0x8 - (std::uint8_t*)exeModule);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio memory address.");
			return;
		}

		fNewWeaponFOV = 2.0f * atanf(tanf(fOriginalWeaponFOV / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio));

		Memory::Write(WeaponFOVScanResult + 0x8, fNewWeaponFOV * fWeaponFOVFactor);
	}
}

DWORD __stdcall Main(void*)
{
	Logging();
	Configuration();
	if (DetectGame())
	{
		FOVFix();
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