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
std::string sFixName = "AdrenalinExtremeShowFOVFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewCameraHFOV;
float fNewCameraFOV;
float fFOVFactor;

// Game detection
enum class Game
{
	AES,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AES, {"Adrenalin: Extreme Show", "Adrenalin.exe"}},
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
			return true;
		}
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
	return false;
}

static SafetyHookMid CameraHFOVInstructionHook{};

void CameraHFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraHFOV]
	}
}

static SafetyHookMid CameraHFOVInstruction2Hook{};

void CameraHFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraHFOV]
	}
}

static SafetyHookMid CameraHFOVInstruction3Hook{};

void CameraHFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraHFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::AES && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewCameraHFOV = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraHFOVInstructionScan1Result = Memory::PatternScan(exeModule, "8B 2D 88 B7 95 00 D9 05 10 BA 95 00");
		if (CameraHFOVInstructionScan1Result)
		{
			spdlog::info("Camera HFOV Instruction Scan 1: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScan1Result + 0x6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScan1Result + 6, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraHFOVInstructionScan1Result + 12, CameraHFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction scan 1 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScan2Result = Memory::PatternScan(exeModule, "D8 4D 74 D9 05 10 BA 95 00");
		if (CameraHFOVInstructionScan2Result)
		{
			spdlog::info("Camera HFOV Instruction Scan 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScan2Result + 0x3 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScan2Result + 3, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstructionScan2Result + 9, CameraHFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction scan 2 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScan3Result = Memory::PatternScan(exeModule, "8B 1D 78 B7 95 00 D9 05 10 BA 95 00");
		if (CameraHFOVInstructionScan3Result)
		{
			spdlog::info("Camera HFOV Instruction Scan 3: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScan3Result + 0x6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScan3Result + 6, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstructionScan3Result + 12, CameraHFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction scan 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 0C BA 95 00 83 C4 F0 8B 15 88 B7 95 00 D8 48 74");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult + 0x2 - (std::uint8_t*)exeModule);
			
			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(0x0095BA0C);

				if (fCurrentCameraFOV != 1.732050776f && fCurrentCameraFOV != 3.23781991f && fCurrentCameraFOV != 2.144506931f && fCurrentCameraFOV != 1.428148031f)
				{
					fCurrentCameraFOV /= fFOVFactor;
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction scan memory address.");
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
