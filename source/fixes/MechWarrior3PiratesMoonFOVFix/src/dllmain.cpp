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
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "MechWarrior3PiratesMoonFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool FixActive;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
float fNewCameraHFOV;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
	return degrees * (fPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
	return radians * (180.0f / fPi);
}

// Game detection
enum class Game
{
	MW3PM,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MW3PM, {"MechWarrior 3: Pirate's Moon", "mech3.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixActive);
	spdlog_confparse(FixActive);

	// Load resolution from ini
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);

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

void FOVFix()
{
	if (eGameType == Game::MW3PM && FixActive == true) {
		std::uint8_t* MW3PM_CameraHFOVScan1Result = Memory::PatternScan(exeModule, "D9 98 EC 00 00 00 D8 0D ?? ?? ?? ?? D9 90 F0 00 00 00");

		std::uint8_t* MW3PM_CameraHFOVScan2Result = Memory::PatternScan(exeModule, "C7 81 38 01 00 00 01 00 00 00 D8 0D ?? ?? ?? ?? C7 81 F8 00 00 00 01 00 00 00");

		std::uint8_t* MW3PM_CameraHFOVScan3Result = Memory::PatternScan(exeModule, "D9 81 EC 00 00 00 D8 0D ?? ?? ?? ?? D9 54 24 08");

		std::uint8_t* MW3PM_CameraHFOVScan4Result = Memory::PatternScan(exeModule, "89 BE F8 00 00 00 D8 0D ?? ?? ?? ?? 89 BE FC 00 00 00");

		std::uint8_t* MW3PM_CameraHFOVScan5Result = Memory::PatternScan(exeModule, "C7 45 D0 00 00 00 00 D8 0D ?? ?? ?? ?? C7 45 D4 00 00 00 00");

		std::uint8_t* MW3PM_CameraHFOVScan6Result = Memory::PatternScan(exeModule, "D9 40 04 D8 0D ?? ?? ?? ?? 8B 45 08");

		std::uint8_t* MW3PM_CodecaveScanResult = Memory::PatternScan(exeModule, "E9 C8 13 FF FF 00 00 00 00 00 00 00 00");

		std::uint8_t* MW3PM_CameraHFOVCodecaveValueAddress = Memory::GetAbsolute(MW3PM_CodecaveScanResult + 0x7);

		Memory::Write(MW3PM_CameraHFOVScan1Result + 0x8, MW3PM_CameraHFOVCodecaveValueAddress - 0x4);

		Memory::Write(MW3PM_CameraHFOVScan2Result + 0xC, MW3PM_CameraHFOVCodecaveValueAddress - 0x4);

		Memory::Write(MW3PM_CameraHFOVScan3Result + 0x8, MW3PM_CameraHFOVCodecaveValueAddress - 0x4);

		Memory::Write(MW3PM_CameraHFOVScan4Result + 0x8, MW3PM_CameraHFOVCodecaveValueAddress - 0x4);

		Memory::Write(MW3PM_CameraHFOVScan5Result + 0x9, MW3PM_CameraHFOVCodecaveValueAddress - 0x4);

		Memory::Write(MW3PM_CameraHFOVScan6Result + 0x5, MW3PM_CameraHFOVCodecaveValueAddress - 0x4);

		fNewCameraHFOV = (2.0f * RadToDeg(atanf(tanf(DegToRad(80.0f / 2.0f)) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / fOldAspectRatio)))) / 160.0f;

		Memory::Write(MW3PM_CodecaveScanResult + 0x7, fNewCameraHFOV);
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
