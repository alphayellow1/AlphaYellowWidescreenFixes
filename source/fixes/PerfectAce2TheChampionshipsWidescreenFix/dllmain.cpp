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
std::string sFixName = "PerfectAce2TheChampionshipsWidescreenFix";
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

// Ini variables
bool bFixActive;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
float fNewGameplayFOV;
float fNewMainMenuFOV;
float fNewCutscenesFOV;
int iNewViewportResWidth2;
int iNewViewportResHeight2;
int iNewViewportResWidth3;
int iNewViewportResHeight3;

// Game detection
enum class Game
{
	PA2TC,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResListUnlock,
	Res_Width_Height
};

enum AspectRatioInstructionsIndices
{
	GameplayAR,
	CutscenesAR
};

enum CameraFOVInstructionsIndices
{
	HFOV,
	VFOV,
	MainMenuFOV,
	CutscenesFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PA2TC, {"Perfect Ace 2: The Championships", "ACEPC.exe"}},
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
static SafetyHookMid GameplayAspectRatioInstructionHook{};
static SafetyHookMid CutscenesAspectRatioInstructionHook{};
static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};
static SafetyHookMid MainMenuFOVInstructionHook{};
static SafetyHookMid CutscenesFOVInstructionHook{};

void GameplayFOVInstructionsMidHook(SafetyHookContext& ctx)
{
	float& fCurrentGameplayFOV = Memory::ReadMem(ctx.ecx + ctx.eax * 0x4 + 0xC0);

	fNewGameplayFOV = fCurrentGameplayFOV * fAspectRatioScale * fFOVFactor;

	FPU::FMUL(fNewGameplayFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::PA2TC && bFixActive == true)
	{
		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "81 FA ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 81 FA", "8B 0F 89 0D");
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

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 B4 81", "8D 4C 24 ?? 51 56 E8 ?? ?? ?? ?? 8D 54 24 ?? 52 56 E8 ?? ?? ?? ?? 8B 44 24");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 8C 81 ?? ?? ?? ?? 52", "D8 8C 81 ?? ?? ?? ?? D8 B4 81", "8B 4C 24 ?? 53 51 B9", "D9 86 ?? ?? ?? ?? 8B 85");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAR] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CutscenesAR] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayAR], 7);

			GameplayAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = Memory::ReadMem(ctx.ecx + ctx.eax * 0x4 + 0x11C);

				fNewAspectRatio2 = fCurrentAspectRatio * fAspectRatioScale;

				FPU::FDIV(fNewAspectRatio2);
			});

			CutscenesAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CutscenesAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesAspectRatio = Memory::ReadMem(ctx.esp + 0x14);

				fCurrentCutscenesAspectRatio = fCurrentCutscenesAspectRatio * fAspectRatioScale;
			});
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Main Menu FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MainMenuFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesFOV] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV], 7);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV], GameplayFOVInstructionsMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV], 7);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV], GameplayFOVInstructionsMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MainMenuFOV], 4);

			MainMenuFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MainMenuFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentMainMenuFOV = Memory::ReadMem(ctx.esp + 0x14);

				fNewMainMenuFOV = fCurrentMainMenuFOV / fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewMainMenuFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV], 6);

			CutscenesFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV = Memory::ReadMem(ctx.esi + 0xA4);

				fNewCutscenesFOV = (fCurrentCutscenesFOV / fAspectRatioScale) / fFOVFactor;

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