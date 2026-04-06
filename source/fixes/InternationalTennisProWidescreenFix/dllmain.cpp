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
std::string sFixName = "InternationalTennisProWidescreenFix";
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

// Ini variables
bool bFixActive;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
float fNewCutscenesAspectRatio;
float fNewGameplayAspectRatio;
float fNewGameplayFOV;
float fNewCutscenesFOV;

// Game detection
enum class Game
{
	ITP,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResListUnlock,
	Res_Width_Height
};

enum AspectRatioInstructionsIndices
{
	CutscenesAR,
	GameplayAR
};

enum CameraFOVInstructionsIndices
{
	GameplayFOV,
	CutscenesFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::ITP, {"International Tennis Pro", "ITPro.exe"}},
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
static SafetyHookMid CutscenesAspectRatioInstructionHook{};
static SafetyHookMid GameplayAspectRatioInstructionHook{};
static SafetyHookMid GameplayCameraFOVInstruction1Hook{};
static SafetyHookMid GameplayCameraFOVInstruction2Hook{};
static SafetyHookMid CutscenesCameraFOVInstructionHook{};

void GameplayFOVInstructionsMidHook(SafetyHookContext& ctx)
{
	float& fCurrentGameplayFOV = Memory::ReadMem(ctx.ecx + ctx.eax * 0x4 + 0xC0);

	if (fCurrentGameplayFOV == 0.4149999917f || fCurrentGameplayFOV == 0.283769995f || fCurrentGameplayFOV == 0.3048000038f)
	{
		fNewGameplayFOV = fCurrentGameplayFOV * fAspectRatioScale * fFOVFactor;
	}
	else
	{
		fNewGameplayFOV = fCurrentGameplayFOV * fAspectRatioScale;
	}

	FPU::FMUL(fNewGameplayFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::ITP && bFixActive == true)
	{
		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "81 FA ?? ?? ?? ?? 0F 8F", "8B 0F 89 0D");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResListUnlock] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Comparison Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResListUnlock] + 65 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[ResListUnlock] + 2, 20000);

			fNewAspectRatio2 = 16.0f;

			Memory::Write(ResolutionInstructionsScansResult[ResListUnlock] + 67, &fNewAspectRatio2);

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res_Width_Height], [](SafetyHookContext& ctx)
			{
				int& iCurrentResX = Memory::ReadMem(ctx.edi);

				int& iCurrentResY = Memory::ReadMem(ctx.edi + 0x4);

				fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
			});
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8D 54 24 ?? 52 56 E8 ?? ?? ?? ?? 8D 44 24", "D8 B4 81");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CutscenesAR] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAR] - (std::uint8_t*)exeModule);

			CutscenesAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CutscenesAR], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esp + 0x14) = *reinterpret_cast<float*>(ctx.esp + 0x14) * fAspectRatioScale;
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayAR], 7);

			GameplayAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayAspectRatio = Memory::ReadMem(ctx.ecx + ctx.eax * 0x4 + 0x11C);

				fNewGameplayAspectRatio = fCurrentGameplayAspectRatio * fAspectRatioScale;

				FPU::FDIV(fNewGameplayAspectRatio);
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 8C 81 ?? ?? ?? ?? 52", "D9 86 ?? ?? ?? ?? 8B 85");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesFOV] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV], 7);

			GameplayCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV], GameplayFOVInstructionsMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV] + 19, 7);

			GameplayCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV] + 19, GameplayFOVInstructionsMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV], 6);

			CutscenesCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV = Memory::ReadMem(ctx.esi + 0xA4);

				fNewCutscenesFOV = fCurrentCutscenesFOV / fAspectRatioScale;

				FPU::FLD(fNewCutscenesFOV);
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