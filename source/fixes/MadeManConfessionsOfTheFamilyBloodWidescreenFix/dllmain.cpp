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
std::string sFixName = "MadeManConfessionsOfTheFamilyBloodWidescreenFix";
std::string sFixVersion = "1.4";
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
float fZoomFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewGeneralFOV;
float fNewGameplayFOV;

// Game detection
enum class Game
{
	MM,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1,
	Res2
};

enum CameraFOVInstructionsIndices
{
	GeneralFOV,
	GameplayFOV_AfterCutscenes1,
	GameplayFOV_AfterCutscenes2,
	HipfireFOV1,
	HipfireFOV2,
	ZoomFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MM, {"Made Man: Confessions of the Family Blood", "IMM.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "ZoomFactor", fZoomFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fZoomFactor);

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

static SafetyHookMid ResolutionWidthInstruction1Hook{};
static SafetyHookMid ResolutionHeightInstruction1Hook{};
static SafetyHookMid ResolutionWidthInstruction2Hook{};
static SafetyHookMid ResolutionHeightInstruction2Hook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid GameplayFOVAfterCutscenesInstruction1Hook{};
static SafetyHookMid GameplayFOVAfterCutscenesInstruction2Hook{};
static SafetyHookMid HipfireFOVInstruction1Hook{};
static SafetyHookMid HipfireFOVInstruction2Hook{};
static SafetyHookMid ZoomFOVInstructionHook{};

void CameraFOVInstructionsMidHook(float& FOVAddress, float fovFactor, SafetyHookContext& ctx)
{
	float& fCurrentGameplayFOV = FOVAddress;

	fNewGameplayFOV = fCurrentGameplayFOV * fovFactor;

	Memory::ReadMem(ctx.eax + 0x48) = fNewGameplayFOV;
}

void WidescreenFix()
{
	if (eGameType == Game::MM && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 81 EC 00 02 00 00", "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 50 51 52 8D 44 24 14");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res1], 6);

			ResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res1] + 6, 6);

			ResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1] + 6, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res2] + 6, 6);

			ResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2] + 6, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res2], 6);

			ResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 10 2D ?? ?? ?? ?? EB 0D 0F BF 50 10 0F BF 78 12");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 8);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.xmm5.f32[0] = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "24 ?? DC 0D ?? ?? ?? ?? 2B C6 85 C0 D9 F2 DD D8 D8 3D ?? ?? ?? ??", "83 48 ?? ?? F3 0F 11 48 ??",
		"41 ?? 83 48 0C ?? F3 0F 11 40 ??", "05 ?? ?? ?? ?? 5B F3 0F 11 40 ??", "C0 F3 0F 10 05 ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? 83 48 0C ?? F3 0F 11 40 ??", "5C C1 F3 0F 59 05 ?? ?? ?? ?? F3 0F 11 40 ??");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GeneralFOV] + 16 - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay FOV After Cutscenes Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOV_AfterCutscenes1] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay FOV After Cutscenes Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOV_AfterCutscenes2] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HipfireFOV1] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HipfireFOV2] + 21 - (std::uint8_t*)exeModule);

			spdlog::info("Zoom FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[ZoomFOV] + 10 - (std::uint8_t*)exeModule);

			fNewGeneralFOV = 1.0f / fAspectRatioScale;

			Memory::Write(CameraFOVInstructionsScansResult[GeneralFOV] + 18, &fNewGeneralFOV);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV_AfterCutscenes1] + 4, 5);

			GameplayFOVAfterCutscenesInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV_AfterCutscenes1] + 4, [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.xmm1.f32[0], fFOVFactor, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV_AfterCutscenes2] + 6, 5);

			GameplayFOVAfterCutscenesInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV_AfterCutscenes2] + 6, [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], fFOVFactor, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HipfireFOV1] + 6, 5);

			HipfireFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireFOV1] + 6, [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], fFOVFactor, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HipfireFOV2] + 21, 5);

			HipfireFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireFOV2] + 21, [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], fFOVFactor, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[ZoomFOV] + 10, 5);

			ZoomFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ZoomFOV] + 10, [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], 1.0f / fZoomFactor, ctx);
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