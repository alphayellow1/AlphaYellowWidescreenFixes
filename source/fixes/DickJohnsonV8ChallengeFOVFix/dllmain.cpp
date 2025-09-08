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
HMODULE thisModule;

// Fix details
std::string sFixName = "DickJohnsonV8ChallengeFOVFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fNewAspectRatio2;
float fAspectRatioScale;
float fNewGameplayFOV1;
float fNewGameplayFOV2;
float fNewPauseMenuFOV;

// Game detection
enum class Game
{
	DJV8C,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DJV8C, {"Dick Johnson V8 Challenge", "Dick Johnson V8 Challenge.exe"}},
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

static SafetyHookMid PauseMenuAspectRatioInstructionHook{};

void PauseMenuAspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio2]
	}
}

static SafetyHookMid GameplayAspectRatioInstruction1Hook{};

void GameplayAspectRatioInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio2]
	}
}

static SafetyHookMid GameplayAspectRatioInstruction2Hook{};

void GameplayAspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio2]
	}
}

void FOVFix()
{
	if (eGameType == Game::DJV8C && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		fNewAspectRatio2 = 0.75f / fAspectRatioScale;

		std::uint8_t* PauseMenuAspectRatioAndCameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "D8 0D B0 15 4A 00 8B 11 68 00 80 BB 44 50 8B 84 24 B4 00 00 00 51 D9 1C 24 50 8B 84 24 B8 00 00 00");
		if (PauseMenuAspectRatioAndCameraFOVInstructionsScanResult)
		{
			spdlog::info("Pause Menu Aspect Ratio & Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), PauseMenuAspectRatioAndCameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(PauseMenuAspectRatioAndCameraFOVInstructionsScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			PauseMenuAspectRatioInstructionHook = safetyhook::create_mid(PauseMenuAspectRatioAndCameraFOVInstructionsScanResult, PauseMenuAspectRatioInstructionMidHook);

			Memory::PatchBytes(PauseMenuAspectRatioAndCameraFOVInstructionsScanResult + 26, "\x90\x90\x90\x90\x90\x90\x90", 7);

			static SafetyHookMid PauseMenuCameraFOVMidHook{};

			PauseMenuCameraFOVMidHook = safetyhook::create_mid(PauseMenuAspectRatioAndCameraFOVInstructionsScanResult + 26, [](SafetyHookContext& ctx)
			{
				float& fCurrentPauseMenuFOV = *reinterpret_cast<float*>(ctx.esp + 0xB8);

				fNewPauseMenuFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentPauseMenuFOV, 1.0f / fAspectRatioScale);

				ctx.eax = std::bit_cast<uintptr_t>(fNewPauseMenuFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate pause menu aspect ratio & camera FOV instructions scan memory address.");
			return;
		}

		std::uint8_t* GameplayAspectRatioAndCameraFOVInstructions1ScanResult = Memory::PatternScan(exeModule, "D8 0D B0 15 4A 00 68 00 80 BB 45 50 8B 84 24 04 02 00 00 51 D9 1C 24 50 8B 84 24 08 02 00 00");
		if (GameplayAspectRatioAndCameraFOVInstructions1ScanResult)
		{
			spdlog::info("Gameplay Aspect Ratio & Camera FOV Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), GameplayAspectRatioAndCameraFOVInstructions1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(GameplayAspectRatioAndCameraFOVInstructions1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);			

			GameplayAspectRatioInstruction1Hook = safetyhook::create_mid(GameplayAspectRatioAndCameraFOVInstructions1ScanResult, GameplayAspectRatioInstruction1MidHook);

			Memory::PatchBytes(GameplayAspectRatioAndCameraFOVInstructions1ScanResult + 24, "\x90\x90\x90\x90\x90\x90\x90", 7);

			static SafetyHookMid GameplayCameraFOV1MidHook{};

			GameplayCameraFOV1MidHook = safetyhook::create_mid(GameplayAspectRatioAndCameraFOVInstructions1ScanResult + 24, [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayFOV1 = *reinterpret_cast<float*>(ctx.esp + 0x208);

				fNewGameplayFOV1 = Maths::CalculateNewFOV_MultiplierBased(fCurrentGameplayFOV1, 1.0f / fAspectRatioScale) / fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewGameplayFOV1);
			});
		}
		else
		{
			spdlog::error("Failed to locate gameplay aspect ratio & camera FOV instructions 1 scan memory address.");
			return;
		}

		// 

		std::uint8_t* GameplayAspectRatioAndCameraFOVInstructions2ScanResult = Memory::PatternScan(exeModule, "8B 01 D8 0D B0 15 4A 00 8B 52 40 52 8B 15 A8 15 4A 00 52 8B 94 24 04 02 00 00 51 D9 1C 24 52 8B 94 24 08 02 00 00");
		if (GameplayAspectRatioAndCameraFOVInstructions2ScanResult)
		{
			spdlog::info("Gameplay Aspect Ratio & Camera FOV Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), GameplayAspectRatioAndCameraFOVInstructions2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(GameplayAspectRatioAndCameraFOVInstructions2ScanResult + 2, "\x90\x90\x90\x90\x90\x90", 6);

			GameplayAspectRatioInstruction2Hook = safetyhook::create_mid(GameplayAspectRatioAndCameraFOVInstructions2ScanResult + 2, GameplayAspectRatioInstruction2MidHook);

			Memory::PatchBytes(GameplayAspectRatioAndCameraFOVInstructions2ScanResult + 31, "\x90\x90\x90\x90\x90\x90\x90", 7);

			static SafetyHookMid GameplayCameraFOV2MidHook{};

			GameplayCameraFOV2MidHook = safetyhook::create_mid(GameplayAspectRatioAndCameraFOVInstructions2ScanResult + 31, [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayFOV2 = *reinterpret_cast<float*>(ctx.esp + 0x208);

				fNewGameplayFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentGameplayFOV2, 1.0f / fAspectRatioScale) / fFOVFactor;

				ctx.edx = std::bit_cast<uintptr_t>(fNewGameplayFOV2);
			});
		}
		else
		{
			spdlog::error("Failed to locate gameplay aspect ratio & camera FOV instructions 2 scan memory address.");
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