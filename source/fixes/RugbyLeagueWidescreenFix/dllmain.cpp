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
#include <algorithm>
#include <cmath>
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "RugbyLeagueWidescreenFix";
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

// Ini variables
bool bFixActive;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fAspectRatioScale;

// Game detection
enum class Game
{
	RL,
	Unknown
};

enum ResolutionInstructionsIndex
{
	ResolutionInstruction1,
	ResolutionInstruction2,
	ResolutionInstruction3,
	ResolutionInstruction4,
	ResolutionInstruction5,
	ResolutionInstruction6,
	GameplayResolutionInstruction1,
	GameplayResolutionInstruction2,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RL, {"Rugby League", "RugbyLeague.exe"}},
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
	if (eGameType == Game::RL && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "6A 20 68 E0 01 00 00 68 80 02 00 00 C7 86 0C 01 00", "FF F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D",
			"E8 26 F9 F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D 44 24 20 81", "CF F1 F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D 4C", "6A 20 BF E0 01 00 00 57 BE 80 02 00 00 56 E8 A8 85 0F", "C8 7D 49 BE 80 02 00 00 BF E0 01 00 00 51 8D 4C 24",
			"89 0D C0 09 71 00 8B 57 04 89 15 C4 09 71 00 C7 05 D4 09 71 00 02 00 00 00", "8B 48 60 DB 41 0C 8B 51 10 89 54 24 14");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{			
			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction1] + 8, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction1] + 3, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction2] + 9, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction2] + 4, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction3] + 11, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction3] + 6, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction4] + 10, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction4] + 5, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction5] + 9, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction5] + 3, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction6] + 4, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction6] + 9, iCurrentResY);

			static SafetyHookMid GameplayResolutionWidthInstruction1MidHook{};

			GameplayResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			static SafetyHookMid GameplayResolutionHeightInstruction1MidHook{};

			GameplayResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction1] + 9, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			static SafetyHookMid GameplayResolutionHeightInstruction2MidHook{};

			GameplayResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction2] + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x10) = iCurrentResY;
			});

			static SafetyHookMid GameplayResolutionWidthInstruction2MidHook{};

			GameplayResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction2] + 3, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0xC) = iCurrentResX;
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 08 D9 05 ?? ?? ?? ?? 89 4E 68 8B 50 04");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90", 2);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax);

				spdlog::info("[Hook] Raw incoming HFOV: {:.12f}", fCurrentCameraHFOV);

				if (fCurrentCameraHFOV != fNewCameraHFOV)
				{
					fNewCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale);
				}				

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionScanResult + 11, "\x90\x90\x90", 3);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 11, [](SafetyHookContext& ctx)
			{
				fNewCameraVFOV = fNewCameraHFOV / fNewAspectRatio;

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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