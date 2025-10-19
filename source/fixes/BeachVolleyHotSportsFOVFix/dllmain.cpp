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
std::string sFixName = "BeachVolleyHotSportsFOVFix";
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
constexpr float fOriginalCameraFOV = 54.43199920654297f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fNewGameplayCameraFOV;
float fNewCutsceneCameraFOV;
double dNewAspectRatio;

// Game detection
enum class Game
{
	BVHS,
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
	{Game::BVHS, {"Beach Volley: Hot Sports", "Game.exe"}},
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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};

static SafetyHookMid CutsceneCameraFOVInstruction1Hook{};
static SafetyHookMid CutsceneCameraFOVInstruction2Hook{};

void CutsceneCameraFOVInstructionMidHook(uintptr_t CameraFOVAddress)
{
	float& fCurrentCutsceneCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

	fNewCutsceneCameraFOV = fCurrentCutsceneCameraFOV * fFOVFactor;

	FPU::FMUL(fNewCutsceneCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::BVHS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		dNewAspectRatio = (double)fNewAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? 83 EC 08 DD 1C 24 8B 45 08 D9 40 18 83 EC 08", "D8 35 ?? ?? ?? ?? 8B 4D ?? D9 99 ?? ?? ?? ?? 8B 55 ?? 52 8B 45 ?? 83 C0 ?? 50 8D 4D ?? 51 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 8B 4D ?? 81 C1 ?? ?? ?? ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 6A ?? 8B 4D ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC CC CC 55", "DD 05 ?? ?? ?? ?? 83 EC 08 DD 1C 24 8B 55 08 D9 42 18 83 EC 08", "D8 35 ?? ?? ?? ?? 8B 4D ?? D9 99 ?? ?? ?? ?? 8B 55 ?? 52 8B 45 ?? 83 C0 ?? 50 8D 4D ?? 51 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 8B 4D ?? 81 C1 ?? ?? ?? ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 6A ?? 8B 4D ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC CC CC CC");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 40 ?? 83 EC ?? DD 1C ?? E8", "D9 41 ?? D8 0D ?? ?? ?? ?? D8 35 ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? 83 C4 ?? D8 3D ?? ?? ?? ?? 8B 55 ?? D9 9A ?? ?? ?? ?? 8B 45 ?? D9 80 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? 8B 4D ?? D9 99 ?? ?? ?? ?? 8B 55 ?? 52 8B 45 ?? 83 C0 ?? 50 8D 4D ?? 51 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 8B 4D ?? 81 C1 ?? ?? ?? ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 6A ?? 8B 4D ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC CC CC CC", "D9 42 ?? 83 EC ?? DD 1C ?? E8", "D9 41 ?? D8 0D ?? ?? ?? ?? D8 35 ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? 83 C4 ?? D8 3D ?? ?? ?? ?? 8B 55 ?? D9 9A ?? ?? ?? ?? 8B 45 ?? D9 80 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? 8B 4D ?? D9 99 ?? ?? ?? ?? 8B 55 ?? 52 8B 45 ?? 83 C0 ?? 50 8D 4D ?? 51 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 8B 4D ?? 81 C1 ?? ?? ?? ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 6A ?? 8B 4D ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC CC CC CC");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio4Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio1Scan], [](SafetyHookContext& ctx) { FPU::FLD(dNewAspectRatio); });

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio2Scan], [](SafetyHookContext& ctx) { FPU::FDIV(fNewAspectRatio); });

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio3Scan], [](SafetyHookContext& ctx) { FPU::FLD(dNewAspectRatio); });

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio4Scan], [](SafetyHookContext& ctx) { FPU::FDIV(fNewAspectRatio); });
		}

		std::uint8_t* GameplayCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "C7 45 FC 5E BA 59 42 8D 4D E4 51 8B 4D B0");
		if (GameplayCameraFOVInstructionScanResult)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			fNewGameplayCameraFOV = fOriginalCameraFOV * fFOVFactor;

			Memory::Write(GameplayCameraFOVInstructionScanResult + 3, fNewGameplayCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate gameplay camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CutsceneCameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "D8 4A 1C 8B 85 70 FE FF FF D9 45 A0 D8 48 50");
		if (CutsceneCameraFOVInstructionsScanResult)
		{
			spdlog::info("Cutscene Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CutsceneCameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CutsceneCameraFOVInstructionsScanResult, "\x90\x90\x90", 3);

			CutsceneCameraFOVInstruction1Hook = safetyhook::create_mid(CutsceneCameraFOVInstructionsScanResult, [](SafetyHookContext& ctx) { CutsceneCameraFOVInstructionMidHook(ctx.edx + 0x1C); });

			Memory::PatchBytes(CutsceneCameraFOVInstructionsScanResult + 12, "\x90\x90\x90", 3);

			CutsceneCameraFOVInstruction2Hook = safetyhook::create_mid(CutsceneCameraFOVInstructionsScanResult + 12, [](SafetyHookContext& ctx) { CutsceneCameraFOVInstructionMidHook(ctx.eax + 0x50); });
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