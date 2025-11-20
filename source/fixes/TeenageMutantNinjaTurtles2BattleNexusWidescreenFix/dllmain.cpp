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
std::string sFixName = "TeenageMutantNinjaTurtles2BattleNexusWidescreenFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
float fNewGameplayCameraFOV;
float fNewCutscenesCameraFOV;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	TMNT2BN,
	Unknown
};

enum ResolutionInstructionsIndex
{
	RendererResolutionScan,
	ViewportResolution1Scan,
	ViewportResolution2Scan
};

enum AspectRatioInstructionsIndex
{
	GameplayAspectRatioScan,
	CutscenesAspectRatioScan
};

enum CameraFOVInsructionsIndex
{
	GameplayCameraFOVScan,
	CutscenesCameraFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TMNT2BN, {"Teenage Mutant Ninja Turtles 2: Battle Nexus", "TMNT2.exe"}},
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

static SafetyHookMid RendererResolutionWidthInstructionHook{};
static SafetyHookMid RendererResolutionHeightInstructionHook{};
static SafetyHookMid ViewportResolutionWidthInstruction1Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction1Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};
static SafetyHookMid GameplayAspectRatioInstructionHook{};
static SafetyHookMid CutscenesAspectRatioInstructionHook{};
static SafetyHookMid GameplayCameraFOVInstructionHook{};
static SafetyHookMid CutscenesCameraFOVInstructionHook{};

void AspectRatioInstructionsMidHook(SafetyHookContext& ctx)
{
	FPU::FMUL(fNewAspectRatio2);
}

void WidescreenFix()
{
	if (eGameType == Game::TMNT2BN && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 08 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? C7 05", "89 46 ?? 8B 3D", "A3 ?? ?? ?? ?? 8B 11 FF 52");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution2Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScansResult[RendererResolutionScan], "\x90\x90", 2);

			RendererResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[RendererResolutionScan] + 8, "\x90\x90\x90", 3);

			RendererResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution1Scan], "\x90\x90\x90", 3);

			ViewportResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 33, "\x90\x90\x90", 3);

			ViewportResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 33, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});

			ViewportResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			ViewportResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 16, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 51 D9 5C 24 ?? 8B 53", "D8 0D ?? ?? ?? ?? 51 D9 5C 24 ?? 8B 50");		
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAspectRatioScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CutscenesAspectRatioScan] - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = 1.0f / fNewAspectRatio;

			Memory::PatchBytes(AspectRatioInstructionsScansResult[GameplayAspectRatioScan], "\x90\x90\x90\x90\x90\x90", 6);

			GameplayAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayAspectRatioScan], AspectRatioInstructionsMidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[CutscenesAspectRatioScan], "\x90\x90\x90\x90\x90\x90", 6);

			CutscenesAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CutscenesAspectRatioScan], AspectRatioInstructionsMidHook);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 63", "D9 46 ?? 8B 46 ?? 85 C0");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayCameraFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesCameraFOVScan] - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[GameplayCameraFOVScan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[GameplayCameraFOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			GameplayCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayCameraFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

				fNewGameplayCameraFOV = (fCurrentGameplayCameraFOV * fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewGameplayCameraFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CutscenesCameraFOVScan], "\x90\x90\x90", 3);

			CutscenesCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesCameraFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x24);

				fNewCutscenesCameraFOV = fCurrentCutscenesCameraFOV * fAspectRatioScale;

				FPU::FLD(fNewCutscenesCameraFOV);
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