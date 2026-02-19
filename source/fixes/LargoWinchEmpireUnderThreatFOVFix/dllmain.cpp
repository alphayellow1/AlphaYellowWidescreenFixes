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
std::string sFixName = "LargoWinchEmpireUnderThreatFOVFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
float fNewHUDFOV;
float fNewGameplayFOV;
float fNewCutscenesFOV;

// Game detection
enum class Game
{
	LWEUT,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	HUD_AR1,
	HUD_AR2,
	HUD_AR3,
	CameraAR
};

enum CameraFOVInstructionsIndices
{
	HUD_FOV1,
	HUD_FOV2,
	HUD_FOV3,
	GameplayFOV,
	CutscenesFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::LWEUT, {"Largo Winch: Empire Under Threat", "LargoWinch.exe"}},
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

static SafetyHookMid CameraAspectRatioInstructionHook{};
static SafetyHookMid GameplayFOVInstructionHook{};
static SafetyHookMid CutscenesFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::LWEUT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 E8 ?? ?? ?? ?? 8B 54 24", "8B 44 24 ?? 8B 4C 24 ?? 50 51 56 E8 ?? ?? ?? ?? 66 8B 15");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("HUD Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUD_AR1] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUD_AR2] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUD_AR3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CameraAR] - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[HUD_AR1] + 1, fNewAspectRatio2);

			Memory::Write(AspectRatioInstructionsScansResult[HUD_AR2] + 1, fNewAspectRatio2);

			Memory::Write(AspectRatioInstructionsScansResult[HUD_AR3] + 1, fNewAspectRatio2);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[CameraAR], 4);

			CameraAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CameraAR], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio2);
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68", "68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D",
		"68 ?? ?? ?? ?? 51 E8 ?? ?? ?? ?? 8B 54 24", "D9 41 ?? D9 54 24", "89 48 ?? C3 90 90 90 90 90 90 90 90 90 90 A0");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("HUD FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HUD_FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("HUD FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HUD_FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("HUD FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HUD_FOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesFOV] - (std::uint8_t*)exeModule);

			fNewHUDFOV = Maths::CalculateNewFOV_RadBased(1.5f, fAspectRatioScale);

			Memory::Write(CameraFOVInstructionsScansResult[HUD_FOV1] + 1, fNewHUDFOV);

			Memory::Write(CameraFOVInstructionsScansResult[HUD_FOV2] + 1, fNewHUDFOV);

			Memory::Write(CameraFOVInstructionsScansResult[HUD_FOV3] + 1, fNewHUDFOV);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV], 3);

			GameplayFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayFOV = *reinterpret_cast<float*>(ctx.ecx + 0x5C);

				fNewGameplayFOV = Maths::CalculateNewFOV_RadBased(fCurrentGameplayFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewGameplayFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV], 3);

			CutscenesFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				const float& fCurrentCutscenesFOV = std::bit_cast<float>(ctx.ecx);

				fNewCutscenesFOV = fCurrentCutscenesFOV / fFOVFactor;

				*reinterpret_cast<float*>(ctx.eax + 0x5C) = fNewCutscenesFOV;
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
