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
#include <cmath> // For atanf, tanf
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
std::string sFixName = "MotocrossManiaWidescreenFix";
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
constexpr float fOriginalMenuFOV = 60.0f;
constexpr float fOriginalRacesFOV2 = 85.0f;

// Ini variables
bool bFixActive;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewMenuFOV;
float fNewRacesFOV1;
float fNewRacesFOV2;
bool firstTime;

// Game detection
enum class Game
{
	MM_V1,
	MM_V2,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResListUnlock,
	ResWidthHeight
};

enum CameraFOVInstructionsScansIndices
{
	Menu,
	Races1,
	Races2
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MM_V1, {"Motocross Mania", "MxMania.exe"}},
	{Game::MM_V2, {"Motocross Mania", "MxManiaUS.exe"}},
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
static SafetyHookMid RacesFOVInstruction1Hook{};

static bool bIsFOVHookInstalled = false;

std::vector<std::uint8_t*> CameraFOVInstructionsScansResult;

void ApplyFOVValues()
{
	fNewMenuFOV = Maths::CalculateNewFOV_DegBased(fOriginalMenuFOV, fAspectRatioScale);

	fNewRacesFOV2 = fOriginalRacesFOV2 * fFOVFactor;
}

void InstallFOVHooks()
{
	if (bIsFOVHookInstalled)
	{
		return;
	}

    CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50", "8B 44 24 ?? 50 8B 84 24",
	"c7 44 24 ?? ?? ?? ?? ?? 0f 85");

	if (!Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult))
	{
		return;
	}

    if (firstTime)
    {
        spdlog::info("Menu Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Menu] - (uint8_t*)exeModule);

        spdlog::info("Races Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Races1] - (uint8_t*)exeModule);

        spdlog::info("Races Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Races2] - (uint8_t*)exeModule);

        Memory::WriteNOPs(CameraFOVInstructionsScansResult[Races1], 4);

        RacesFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Races1], [](SafetyHookContext& ctx)
        {
            float& fCurrentRacesFOV1 = Memory::ReadMem(ctx.esp + 0x28);

            fNewRacesFOV1 = Maths::CalculateNewFOV_DegBased(fCurrentRacesFOV1, fAspectRatioScale);

            ctx.eax = std::bit_cast<uintptr_t>(fNewRacesFOV1);
        });

        firstTime = false;
    }

	bIsFOVHookInstalled = true;
}

void SetFOV()
{
	ApplyFOVValues();
	InstallFOVHooks();

	if (CameraFOVInstructionsScansResult[Menu] != nullptr && CameraFOVInstructionsScansResult[Races2] != nullptr)
	{
		Memory::Write(CameraFOVInstructionsScansResult[Menu] + 1, fNewMenuFOV);

		Memory::Write(CameraFOVInstructionsScansResult[Races2] + 4, fNewRacesFOV2);
	}
}

void WidescreenFix()
{
	if ((eGameType == Game::MM_V1 || eGameType == Game::MM_V2) && bFixActive == true)
	{
		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "0F 82 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 87 ?? ?? ?? ?? 81 F9",
		"8B 75 ?? 8B 7D ?? 8B D6");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResListUnlock] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResWidthHeight] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock], 6);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 11, 6);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 23, 6);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 35, 6);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 56, 6);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 77, 6);

			firstTime = true;

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.ebp + 0x8);

				int& iCurrentHeight = Memory::ReadMem(ctx.ebp + 0xC);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				SetFOV();
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
