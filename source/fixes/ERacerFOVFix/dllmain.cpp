// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>
#include <vector>
#include <algorithm>
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

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "ERacerFOVFix";
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
constexpr float fOriginalMenuHFOV = 0.9424778223f;

// Ini variables
bool bFixActive;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewGameplayHFOV;
float fNewGameplayVFOV;
float fNewCarSelectionHFOV;

// Game detection
enum class Game
{
	ERACER,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	GameplayHFOV,
	GameplayVFOV,
	MenuHFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::ERACER, {"E-Racer", "ERACER.EXE"}},
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(fFOVFactor);

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

static SafetyHookMid ResolutionInstructionsHook{};
static SafetyHookMid GameplayHFOVInstructionHook{};
static SafetyHookMid GameplayVFOVInstructionHook{};
static SafetyHookMid CarSelectionHFOVInstructionHook{};

void FOVFix()
{
	if (bFixActive == true && eGameType == Game::ERACER)
	{
		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "8B 44 24 ?? 8B 4C 24 ?? A3 ?? ?? ?? ?? 48");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);

				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				ResolutionInstructionsHook.disable();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 4C 24 0C 8B 86 98 00 00 00", "8B 4C 24 0C 8B 86 9C 00 00 00",
		"C7 86 ?? ?? ?? ?? ?? ?? ?? ?? C7 86 ?? ?? ?? ?? ?? ?? ?? ?? C7 86 ?? ?? ?? ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? 89 ?? ?? ?? ?? ?? 89");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayHFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayVFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Car Selection HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MenuHFOV] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayHFOV], 4);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayVFOV], 4);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MenuHFOV], 10);

			GameplayHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayHFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayHFOV = Memory::ReadMem(ctx.esp + 0xC);

				fNewGameplayHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentGameplayHFOV, fAspectRatioScale, fFOVFactor);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewGameplayHFOV);
			});

			GameplayVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayVFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayVFOV = Memory::ReadMem(ctx.esp + 0xC);

				fNewGameplayVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentGameplayVFOV, fFOVFactor);
				
				ctx.ecx = std::bit_cast<uintptr_t>(fNewGameplayVFOV);
			});

			CarSelectionHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MenuHFOV], [](SafetyHookContext& ctx)
			{
				fNewCarSelectionHFOV = Maths::CalculateNewFOV_RadBased(fOriginalMenuHFOV, fAspectRatioScale);

				*reinterpret_cast<float*>(ctx.esi + 0x98) = fNewCarSelectionHFOV;
			});
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