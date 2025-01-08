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
#include <tlhelp32.h>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE dllModule = nullptr;
HMODULE dllModule2;
HMODULE thisModule;
HANDLE monitoringThread = NULL;
volatile BOOL stopMonitoring = FALSE;
volatile BOOL isFgamex86Loaded = FALSE;
CRITICAL_SECTION cs; // Critical section for synchronization
HMODULE hModule;

// Fix details
std::string sFixName = "AmericanMcGeesAliceFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float fOriginalCameraFOV = 90.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
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
	AMA,
	QUAKE3,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AMA, {"American McGee's Alice", "alice.exe"}},
	{Game::QUAKE3, {"American McGee's Alice", "quake3.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
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
		}
	}

	return true;
}

void FOVFix()
{
	if (eGameType == Game::AMA || eGameType == Game::QUAKE3 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewCameraFOV = 2.0f * RadToDeg(atanf(tanf(DegToRad(fOriginalCameraFOV / 2.0f)) * (fNewAspectRatio / oldAspectRatio)));

		std::uint8_t* GameplayCameraFOVScanResult = Memory::PatternScan(hModule, "C7 45 EC ?? ?? ?? ?? EB 17 D9");
		if (GameplayCameraFOVScanResult)
		{
			spdlog::info("Gameplay Camera FOV: Address is fgamex86.dll+{:x}", GameplayCameraFOVScanResult + 0x3 - (std::uint8_t*)hModule);

			Memory::Write(GameplayCameraFOVScanResult + 0x3, fNewCameraFOV * fFOVFactor);
		}
		else
		{
			spdlog::error("Failed to locate gameplay FOV memory address.");
			return;
		}

		std::uint8_t* CutscenesCameraFOVScanResult = Memory::PatternScan(hModule, "88 18 B9 ?? ?? ?? ?? B8 00 00");
		if (CutscenesCameraFOVScanResult)
		{
			spdlog::info("Cutscenes Camera FOV: Address is fgamex86.dll+{:x}", CutscenesCameraFOVScanResult + 0x3 - (std::uint8_t*)hModule);

			Memory::Write(CutscenesCameraFOVScanResult + 0x3, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate cutscenes FOV memory address.");
			return;
		}
	}
}

DWORD __stdcall Main(void*)
{
	if (DetectGame())
	{
		FOVFix();
	}
	return TRUE;
}

DWORD WINAPI MonitorThread(LPVOID lpParameter)
{
	Logging();
	Configuration();

	while (!stopMonitoring)
	{
		// Checks if fgamex86.dll is loaded
		hModule = GetModuleHandleA("fgamex86.dll");
		if (hModule != NULL)
		{
			// Ensure thread safety when calling Main
			EnterCriticalSection(&cs);
			Main(NULL);
			LeaveCriticalSection(&cs);
		}

		// Sleeps for 1 second before checking if the DLL is still loaded
		Sleep(1000); // Checks every second
	}

	return 0;
}

extern "C" __declspec(dllexport) void StartMonitoring()
{
	if (monitoringThread == NULL)
	{
		stopMonitoring = FALSE;
		InitializeCriticalSection(&cs);
		monitoringThread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
		if (monitoringThread)
		{
			SetThreadPriority(monitoringThread, THREAD_PRIORITY_HIGHEST);
			CloseHandle(monitoringThread);
		}
	}
}

extern "C" __declspec(dllexport) void StopMonitoring()
{
	if (monitoringThread != NULL)
	{
		stopMonitoring = TRUE;
		WaitForSingleObject(monitoringThread, INFINITE);
		DeleteCriticalSection(&cs);
		monitoringThread = NULL;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		thisModule = hModule;
		StartMonitoring();
		break;
	case DLL_PROCESS_DETACH:
		StopMonitoring();
		break;
	}
	return TRUE;
}