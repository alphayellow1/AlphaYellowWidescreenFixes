﻿// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#pragma warning(push)
#pragma warning(disable:6385) 
#pragma warning(disable:26498)
#pragma warning(disable:26814)
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <safetyhook.hpp>
#pragma warning(pop)
#include <inipp/inipp.h>
#include <vector>
#include <map>
#include <windows.h>
#include <psapi.h> // For GetModuleInformation
#include <fstream>
#include <filesystem>
#include <cmath> // For atanf, tanf
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <tlhelp32.h>
#include <winternl.h>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE dllModule = nullptr;
HMODULE thisModule = nullptr;
HMODULE hModule = nullptr;

// Fix details
std::string sFixName = "SupercarStreetChallengeFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewCameraFOV;
float fFOVFactor;
float fAspectRatioScale;
float fNewAspectRatio2;

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
	SSC,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SSC, {"Supercar Street Challenge", "launchSSC.exe"}},
};

const GameInfo* game = nullptr;
Game eGameType = Game::Unknown;

// Typedefs for loader notification API
using LdrRegisterDllNotification_t =
NTSTATUS(NTAPI*)(ULONG, PVOID, PVOID, PVOID*);
using LdrUnregisterDllNotification_t =
NTSTATUS(NTAPI*)(PVOID);

static PVOID g_dllNotifyCookie = nullptr;

void Logging()
{
	// Determine DLL folder and game‑exe folder (unchanged) …
	WCHAR dllPath[_MAX_PATH] = { 0 };
	GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
	sFixPath = dllPath; sFixPath = sFixPath.remove_filename();

	WCHAR exePathW[_MAX_PATH] = { 0 };
	GetModuleFileNameW(exeModule, exePathW, MAX_PATH);
	sExePath = exePathW;
	sExeName = sExePath.filename().string();
	sExePath = sExePath.remove_filename();

	// Create logger exactly once
	static bool loggerInitialized = false;
	if (!loggerInitialized)
	{
		// If already exists in registry (shouldn’t on first call), drop it
		if (spdlog::get(sFixName))
			spdlog::drop(sFixName);

		logger = spdlog::basic_logger_st(
			sFixName,
			sExePath.string() + "\\" + sLogFile,
			/*truncate=*/ true                                                   // overwrite old log file
		);

		spdlog::set_default_logger(logger);
		spdlog::flush_on(spdlog::level::debug);
		spdlog::set_level(spdlog::level::debug);

		// Initial header
		spdlog::info("----------");
		spdlog::info("{:s} v{:s} loaded.", sFixName, sFixVersion);
		spdlog::info("Log file: {}", sExePath.string() + "\\" + sLogFile);
		spdlog::info("Module Name: {0:s}", sExeName);
		spdlog::info("Module Path: {0:s}", sExePath.string());
		spdlog::info("Module Address: 0x{0:X}", reinterpret_cast<uintptr_t>(exeModule));
		spdlog::info("----------");
		spdlog::info("DLL has been successfully initialized.");

		loggerInitialized = true;
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
			return true;
		}
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
	return false;
}

void LogModuleEvent(bool loaded, const std::wstring& dllName)
{
	// Convert to narrow string for spdlog
	std::string name(dllName.begin(), dllName.end());

	if (loaded)
	{
		spdlog::info("{} loaded.", name);
	}
	else
	{
		spdlog::info("{} unloaded.", name);
	}
}

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * (atanf(tanf(fCurrentFOV / 2.0f) * fAspectRatioScale));
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ecx + 0x14);

	if (fCurrentCameraFOV == 1.047197699546814f)
	{
		fCurrentCameraFOV = CalculateNewFOV(fCurrentCameraFOV) * fFOVFactor;
	}
}

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.ecx + 0x30);

	if (fCurrentAspectRatio == 1.1428571939468384f)
	{
		fCurrentAspectRatio *= fAspectRatioScale;
	}	
}

void FOVFix()
{
	if (eGameType == Game::SSC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;		

		std::uint8_t* AspectRatioAndCameraFOVInstructionsScanResult = Memory::PatternScan(hModule, "D8 71 30 D9 51 34 D9 41 14 D8 0D ?? ?? ?? ?? D9 F2");
		if (AspectRatioAndCameraFOVInstructionsScanResult)
		{
			std::uintptr_t offset = reinterpret_cast<std::uintptr_t>(AspectRatioAndCameraFOVInstructionsScanResult) - reinterpret_cast<std::uintptr_t>(dllModule);

			char namebuf[MAX_PATH] = { 0 };

			GetModuleFileNameA(hModule, namebuf, MAX_PATH);

			spdlog::info("Aspect Ratio and Camera FOV Instructions: Address is {}+0x{:x}", namebuf, offset);

			CameraFOVInstructionHook = safetyhook::create_mid(AspectRatioAndCameraFOVInstructionsScanResult + 6, CameraFOVInstructionMidHook);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioAndCameraFOVInstructionsScanResult, AspectRatioInstructionMidHook);					
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio and camera FOV memory address.");
			return;
		}
	}
}

// — Loader‑notification integration —
enum {
	LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
	LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2
};

typedef struct _UNICODE_STR {
	USHORT Length, MaximumLength;
	PWSTR  Buffer;
} UNICODE_STR, * PUNICODE_STR;

typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA {
	ULONG Flags;
	PUNICODE_STR FullDllName, BaseDllName;
	PVOID DllBase;
	ULONG SizeOfImage;
} LDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA;

typedef struct _LDR_DLL_UNLOADED_NOTIFICATION_DATA {
	ULONG Flags;
	PUNICODE_STR FullDllName, BaseDllName;
	PVOID DllBase;
	ULONG SizeOfImage;
} LDR_DLL_UNLOADED_NOTIFICATION_DATA, * PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

typedef union _LDR_DLL_NOTIFICATION_DATA {
	LDR_DLL_LOADED_NOTIFICATION_DATA   Loaded;
	LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
} LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;

VOID CALLBACK DllNotification(
    ULONG Reason,
    PLDR_DLL_NOTIFICATION_DATA Data,
    PVOID)
{
    // Get the base DLL filename in lowercase
    std::wstring baseName(Data->Loaded.BaseDllName->Buffer,
                          Data->Loaded.BaseDllName->Length / sizeof(WCHAR));
    std::transform(baseName.begin(), baseName.end(), baseName.begin(), ::towlower);

    bool isTarget = (baseName == L"ccrg.dll" || baseName == L"ssc.dll");

	if (Reason == LDR_DLL_NOTIFICATION_REASON_LOADED && isTarget)
	{
		hModule = reinterpret_cast<HMODULE>(Data->Loaded.DllBase);
		
		static bool initialized = false;

		if (!initialized)
		{
			Logging();       // create the spdlog file sink
			Configuration(); // read your INI
			DetectGame();    // fill eGameType, game, etc.
			initialized = true;
		}

		LogModuleEvent(true, baseName);
		FOVFix();  // detailed logs happen inside here now
	}
	else if (Reason == LDR_DLL_NOTIFICATION_REASON_UNLOADED && isTarget)
	{
		LogModuleEvent(false, baseName);
	}
}

void InstallDllNotification()
{
	HMODULE ntdll = ::GetModuleHandleA("ntdll.dll");

	if (!ntdll) {
		spdlog::error("GetModuleHandleA(\"ntdll.dll\") failed: 0x{:X}", GetLastError());
		return;  // or handle error appropriately
	}

	auto Reg = reinterpret_cast<LdrRegisterDllNotification_t>(
		::GetProcAddress(ntdll, "LdrRegisterDllNotification"));

	if (!Reg) {
		spdlog::error("GetProcAddress(LdrRegisterDllNotification) failed: 0x{:X}", GetLastError());
		return;
	}

	Reg(0, DllNotification, nullptr, &g_dllNotifyCookie);
}

void RemoveDllNotification()
{
	// 1) Get handle to ntdll.dll and verify it succeeded
	HMODULE ntdll = ::GetModuleHandleA("ntdll.dll");

	if (!ntdll)
	{
		spdlog::error("GetModuleHandleA(\"ntdll.dll\") failed: 0x{:#x}",
			static_cast<uint32_t>(GetLastError()));
		return;
	}

	// 2) Retrieve LdrUnregisterDllNotification and verify it succeeded
	auto Unreg = reinterpret_cast<LdrUnregisterDllNotification_t>(
		::GetProcAddress(ntdll, "LdrUnregisterDllNotification"));

	if (!Unreg)
	{
		spdlog::error("GetProcAddress(\"LdrUnregisterDllNotification\") failed: 0x{:#x}",
			static_cast<uint32_t>(GetLastError()));
		return;
	}

	// 3) Call the unregister function
	NTSTATUS status = Unreg(g_dllNotifyCookie);

	if (!NT_SUCCESS(status))
	{
		spdlog::error("LdrUnregisterDllNotification returned 0x{:#x}", status);
	}
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		thisModule = hMod;
		dllModule = exeModule;
		InstallDllNotification();
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		RemoveDllNotification();
	}
	return TRUE;
}