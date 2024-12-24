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
std::string sFixName = "BarbieExplorerWidescreenFix";
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixFOV = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
float fNewCameraHFOV;
float fNewCameraFOV;
float fFOVFactor;

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
	BE,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BE, {"Barbie Explorer", "Barbiex.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixFOV);
	spdlog_confparse(FixFOV);

	// Load resolution from ini
	inipp::get_value(ini.sections["Resolution"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Resolution"], "Height", iCurrentResY);
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

void Fix()
{
	if (eGameType == Game::BE) {
		std::uint8_t* BE_ResolutionWidthScanResult = Memory::PatternScan(exeModule, "00 80 02 00 00 C7");

		std::uint8_t* BE_ResolutionHeightScanResult = Memory::PatternScan(exeModule, "00 E0 01 00 00 C7");

		std::uint8_t* BE_ForegroundHFOVScanResult = Memory::PatternScan(exeModule, "DB 45 D8 D8 0D 60 BD 5D 00 D8 0D 68 BD 5D 00");

		Memory::PatchBytes(BE_ForegroundHFOVScanResult, "\xE9\xF7\x21\x04\x00\x90\x90\x90\x90", 9);

		std::uint8_t* BE_BackgroundHFOVScanResult = Memory::PatternScan(exeModule, "D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D 94 BD 5D 00 D9 05 64 BD 5D 00 D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D 98 BD 5D 00 D9 05 68 BD 5D 00 D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D A0 BD 5D 00 DB 05 58 BE 5D 00 D8 05 94 BD 5D 00 D9 1D 60 BD 5D 00 DB 05 64 BE 5D 00 D8 05 98 BD 5D 00 D9 1D 64 BD 5D 00 DB 05 54 BE 5D 00 D8 05 A0 BD 5D 00 D9 15 68 BD 5D 00 D8 1D 00 F3 48 00 DF E0 F6 C4 01 74 16 C7 05 68 BD 5D 00 00 00 00 00 A1 28 BD 5D 00 0C 24 A3 28 BD 5D 00 D9 05 68 BD 5D 00 D8 1D 24 F3 48 00 DF E0 F6 C4 41 75 19 C7 05 68 BD 5D 00 00 FF 7F 47 8B 0D 28 BD 5D 00 83 C9 24 89 0D 28 BD 5D 00 0F BF 05 F2 BD 5D 00 99 2B C2 D1 F8 89 45 BC DB 45 BC D8 1D 68 BD 5D 00 DF E0 F6 C4 01 74 6E 8B 15 68 BD 5D 00 89 15 1C BE 5D 00 D9 05 10 F3 48 00 D8 35");

		Memory::PatchBytes(BE_BackgroundHFOVScanResult + 0x2, "\xB2\xE5", 2);

		std::uint8_t* BE_CameraHFOVCodecaveScanResult = Memory::PatternScan(exeModule, "E9 72 DF FF FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");

		Memory::PatchBytes(BE_CameraHFOVCodecaveScanResult + 0x7, "\xDB\x45\xD8\xD8\x0D\xB2\xE5\x48\x00\xD8\x0D\x60\xBD\x5D\x00\xE9\xF9\xDD\xFB\xFF", 20);

		std::uint8_t* BE_CameraNewHFOVCodecaveScanResult = Memory::PatternScan(exeModule, "DB 45 D8 D8 0D B2 E5 48 00 D8 0D 60 BD 5D 00 E9 F9 DD FB FF");

		fNewCameraHFOV = (4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY));

		Memory::Write(BE_CameraNewHFOVCodecaveScanResult + 0x14, fNewCameraHFOV);

		Memory::Write(BE_ResolutionWidthScanResult + 0x1, iCurrentResX);

		Memory::Write(BE_ResolutionHeightScanResult + 0x1, iCurrentResY);
	}
}

DWORD __stdcall Main(void*)
{
	Logging();
	Configuration();
	if (DetectGame())
	{
		Fix();
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