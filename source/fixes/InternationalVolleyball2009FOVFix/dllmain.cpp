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
HMODULE thisModule;

// Fix details
std::string sFixName = "InternationalVolleyball2009FOVFix";
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
float fNewGameplayCameraFOV;
float fNewCutsceneCameraFOV1;
float fNewCutsceneCameraFOV2;
double dNewAspectRatio;
uint8_t* GameplayCameraFOVAddress;

// Game detection
enum class Game
{
	IV2009,
	Unknown
};

enum AspectRatioInstructionsIndex
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan,
	AspectRatio4Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::IV2009, {"International Volleyball 2009", "IV2009.exe"}},
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
	Sleep(500);

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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};

static SafetyHookMid GameplayCameraFOVInstructionHook{};
static SafetyHookMid CutsceneCameraFOVInstruction1Hook{};
static SafetyHookMid CutsceneCameraFOVInstruction2Hook{};

void AspectRatioInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld qword ptr ds:[dNewAspectRatio]
	}
}

void AspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fdiv qword ptr ds:[dNewAspectRatio]
	}
}

void GameplayCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentGameplayCameraFOV = *reinterpret_cast<float*>(GameplayCameraFOVAddress);
	
	fNewGameplayCameraFOV = fCurrentGameplayCameraFOV * fFOVFactor;
	
	_asm
	{
		fld dword ptr ds:[fNewGameplayCameraFOV]
	}
}

void CutsceneCameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCutsceneCameraFOV1 = *reinterpret_cast<float*>(ctx.esi + 0x50);

	fNewCutsceneCameraFOV1 = fCurrentCutsceneCameraFOV1 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCutsceneCameraFOV1]
	}
}

void CutsceneCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCutsceneCameraFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x1C);

	fNewCutsceneCameraFOV2 = fCurrentCutsceneCameraFOV2 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCutsceneCameraFOV2]
	}
}

void FOVFix()
{
	if (eGameType == Game::IV2009 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		dNewAspectRatio = (double)fNewAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? DD 5C 24 ?? D9 46 ?? DD 1C ?? E8 ?? ?? ?? ?? 8B 5C 24", "DC 35 ?? ?? ?? ?? D9 9B ?? ?? ?? ?? D9 46", "DC 35 ?? ?? ?? ?? D9 9B ?? ?? ?? ?? D9 05", "DD 05 ?? ?? ?? ?? DD 5C 24 ?? D9 46 ?? DD 1C ?? E8 ?? ?? ?? ?? 8B 5C 24");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio4Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio1Scan], AspectRatioInstruction1MidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio2Scan], AspectRatioInstruction2MidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio3Scan], AspectRatioInstruction2MidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio4Scan], AspectRatioInstruction1MidHook);
		}

		std::uint8_t* GameplayCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? 8D 44 24 ?? 50 D9 5C 24 ?? 8B CD");
		if (GameplayCameraFOVInstructionScanResult)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			GameplayCameraFOVAddress = Memory::GetPointer<uint32_t>(GameplayCameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(GameplayCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			GameplayCameraFOVInstructionHook = safetyhook::create_mid(GameplayCameraFOVInstructionScanResult, GameplayCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate gameplay camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CutsceneCameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "D9 46 ?? D8 CA D9 46 ?? D8 CA DE C1 D9 5C 24 ?? D9 46 ?? D8 CA");
		if (CutsceneCameraFOVInstructionsScanResult)
		{
			spdlog::info("Cutscene Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CutsceneCameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CutsceneCameraFOVInstructionsScanResult, "\x90\x90\x90", 3);

			CutsceneCameraFOVInstruction1Hook = safetyhook::create_mid(CutsceneCameraFOVInstructionsScanResult, CutsceneCameraFOVInstruction1MidHook);

			Memory::PatchBytes(CutsceneCameraFOVInstructionsScanResult + 5, "\x90\x90\x90", 3);

			CutsceneCameraFOVInstruction2Hook = safetyhook::create_mid(CutsceneCameraFOVInstructionsScanResult + 5, CutsceneCameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate cutscene camera FOV instructions scan memory address.");
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