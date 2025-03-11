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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraHFOV;
float fNewCameraFOV;
float fFOVFactor;
float fNewAspectRatio;

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
	inipp::get_value(ini.sections["WidescreenFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

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

static SafetyHookMid CameraForegroundHFOVHook{};

void CameraForegroundHFOVMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

static SafetyHookMid CameraBackgroundHFOVHook{};

void CameraBackgroundHFOVMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::BE && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionWidthScanResult = Memory::PatternScan(exeModule, "00 80 02 00 00 C7");
		if (ResolutionWidthScanResult)
		{
			spdlog::info("Resolution Width: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthScanResult + 1 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionWidthScanResult + 1, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution width memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightScanResult = Memory::PatternScan(exeModule, "00 E0 01 00 00 C7");
		if (ResolutionHeightScanResult)
		{
			spdlog::info("Resolution Height: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightScanResult + 1 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionHeightScanResult + 1, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution height memory address.");
			return;
		}

		fNewCameraHFOV = fOldAspectRatio / fNewAspectRatio;

		std::uint8_t* CameraForegroundHFOVInstructionScanResult = Memory::PatternScan(exeModule, "DB 45 D8 D8 0D 60 BD 5D 00 D8 0D 68 BD 5D 00");
		if (CameraForegroundHFOVInstructionScanResult)
		{
			spdlog::info("Camera Foreground HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraForegroundHFOVInstructionScanResult + 3 - (std::uint8_t*)exeModule);

			CameraForegroundHFOVHook = safetyhook::create_mid(CameraForegroundHFOVInstructionScanResult + 3, CameraForegroundHFOVMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera foreground HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraBackgroundHFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D 94 BD 5D 00 D9 05 64 BD 5D 00 D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D 98 BD 5D 00 D9 05 68 BD 5D 00 D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D A0 BD 5D 00 DB 05 58 BE 5D 00 D8 05 94 BD 5D 00 D9 1D 60 BD 5D 00 DB 05 64 BE 5D 00 D8 05 98 BD 5D 00 D9 1D 64 BD 5D 00 DB 05 54 BE 5D 00 D8 05 A0 BD 5D 00 D9 15 68 BD 5D 00 D8 1D 00 F3 48 00 DF E0 F6 C4 01 74 16 C7 05 68 BD 5D 00 00 00 00 00 A1 28 BD 5D 00 0C 24 A3 28 BD 5D 00 D9 05 68 BD 5D 00 D8 1D 24 F3 48 00 DF E0 F6 C4 41 75 19 C7 05 68 BD 5D 00 00 FF 7F 47 8B 0D 28 BD 5D 00 83 C9 24 89 0D 28 BD 5D 00 0F BF 05 F2 BD 5D 00 99 2B C2 D1 F8 89 45 BC DB 45 BC D8 1D 68 BD 5D 00 DF E0 F6 C4 01 74 6E 8B 15 68 BD 5D 00 89 15 1C BE 5D 00 D9 05 10 F3 48 00 D8 35");
		if (CameraBackgroundHFOVInstructionScanResult)
		{
			spdlog::info("Camera Background HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraBackgroundHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraBackgroundHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraBackgroundHFOVHook = safetyhook::create_mid(CameraBackgroundHFOVInstructionScanResult + 6, CameraBackgroundHFOVMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera background HFOV instruction memory address.");
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