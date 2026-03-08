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
std::string sFixName = "KnightsOfTheTemple2WidescreenFix";
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
constexpr float fOriginalGameplayAspectRatio = 0.75f;
constexpr float fOriginalGameplayFOV = 1.5707963268f; // 90 degrees in radians

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
float fNewMenuAndCutscenesCameraAspectRatio;
float fNewMenuAndCutscenesCameraFOV;
float fNewBlackBarsValue;
float fNewBlackBarsValue1;

// Game detection
enum class Game
{
	KOTT2,
	Unknown
};

enum ResolutionListsIndices
{
	List1,
	List2
};

enum AspectRatioInstructionsIndices
{
	Menu_Cutscenes_AR,
	Gameplay_AR
};

enum CameraFOVInstructionsIndices
{
	Menu_Cutscenes_FOV,
	Gameplay_FOV
};

enum BlackBarsAdjustInstructionsIndices
{
	BlackBars1,
	BlackBars2,
	BlackBars3,
	BlackBars4
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::KOTT2, {"Knights of the Temple II", "KOTT2.exe"}},
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

static SafetyHookMid MenuAndCutscenesAspectRatioInstructionHook{};
static SafetyHookMid MenuAndCutscenesCameraFOVInstructionHook{};
static SafetyHookMid BlackBarsAdjustInstruction1Hook{};
static SafetyHookMid BlackBarsAdjustInstruction2Hook{};
static SafetyHookMid BlackBarsAdjustInstruction3Hook{};
static SafetyHookMid BlackBarsAdjustInstruction4Hook{};

void BlackBarsAdjustInstructionsMidHook(SafetyHookContext& ctx)
{
	float& fCurrentBlackBarsValue = Memory::ReadMem(ctx.esi + 0x610);

	fNewBlackBarsValue = fCurrentBlackBarsValue * fAspectRatioScale;

	FPU::FLD(fNewBlackBarsValue);
}

void WidescreenFix()
{
	if (eGameType == Game::KOTT2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionListsScansResult = Memory::PatternScan(exeModule, "3D ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 0F 85", "C7 86 44 C2 00 00 80 02 00 00");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[List1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution List 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[List2] - (std::uint8_t*)exeModule);

			// Resolution List 1
			// 640x480
			Memory::Write(ResolutionListsScansResult[List1] + 1, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List1] + 11, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[List1] + 29, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List1] + 39, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[List1] + 53, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List1] + 63, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListsScansResult[List1] + 77, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List1] + 87, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionListsScansResult[List1] + 101, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List1] + 111, iCurrentResY);

			// 1920x1440
			Memory::Write(ResolutionListsScansResult[List1] + 125, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List1] + 135, iCurrentResY);

			// Resolution List 2
			// 640x480
			Memory::Write(ResolutionListsScansResult[List2] + 6, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List2] + 16, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[List2] + 26, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List2] + 36, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[List2] + 46, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List2] + 56, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListsScansResult[List2] + 66, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List2] + 76, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionListsScansResult[List2] + 86, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List2] + 96, iCurrentResY);

			// 1920x1440
			Memory::Write(ResolutionListsScansResult[List2] + 106, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[List2] + 116, iCurrentResY);
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8B 88 ?? ?? ?? ?? 89 4E", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? D9 1C 24 8B 8C 24");

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 90 ?? ?? ?? ?? 89 56 ?? 8B 88 ?? ?? ?? ?? 89 4E", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? D9 1C 24 8B 8C 24");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Menu & Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[Menu_Cutscenes_AR] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[Gameplay_AR] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[Menu_Cutscenes_AR], 6);

			MenuAndCutscenesAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[Menu_Cutscenes_AR], [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuAndCutscenesAspectRatio = Memory::ReadMem(ctx.eax + 0x90);

				if (fCurrentMenuAndCutscenesAspectRatio == 0.75f)
				{
					fNewMenuAndCutscenesCameraAspectRatio = fCurrentMenuAndCutscenesAspectRatio / fAspectRatioScale;
				}
				else
				{
					fNewMenuAndCutscenesCameraAspectRatio = fCurrentMenuAndCutscenesAspectRatio;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewMenuAndCutscenesCameraAspectRatio);
			});

			fNewAspectRatio2 = fOriginalGameplayAspectRatio / fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[Gameplay_AR] + 1, fNewAspectRatio2);
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Menu & Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Menu_Cutscenes_FOV] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Gameplay_FOV] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Menu_Cutscenes_FOV], 6);

			MenuAndCutscenesCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Menu_Cutscenes_FOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuAndCutscenesCameraFOV = Memory::ReadMem(ctx.eax + 0x8C);

				fNewMenuAndCutscenesCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentMenuAndCutscenesCameraFOV, fAspectRatioScale);

				ctx.edx = std::bit_cast<uintptr_t>(fNewMenuAndCutscenesCameraFOV);
			});

			fNewGameplayCameraFOV = Maths::CalculateNewFOV_RadBased(fOriginalGameplayFOV, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[Gameplay_FOV] + 1, fNewGameplayCameraFOV);
		}

		std::vector<std::uint8_t*> BlackBarsAdjustInstructionsScansResult = Memory::PatternScan(exeModule, "D8 B6 ?? ?? ?? ?? D9 96 ?? ?? ?? ?? D9 44 24",
		"D9 86 ?? ?? ?? ?? D8 B6 ?? ?? ?? ?? 74 ?? D8 0D ?? ?? ?? ?? 8B 4C 24", "D9 86 ?? ?? ?? ?? D8 B6 ?? ?? ?? ?? 74 ?? D8 0D ?? ?? ?? ?? D9 05",
		"D9 86 ?? ?? ?? ?? D8 B6 ?? ?? ?? ?? 74 ?? D8 0D ?? ?? ?? ?? 8B 8E");
		if (Memory::AreAllSignaturesValid(BlackBarsAdjustInstructionsScansResult) == true)
		{
			spdlog::info("Black Bars Adjust Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), BlackBarsAdjustInstructionsScansResult[BlackBars1] - (std::uint8_t*)exeModule);

			spdlog::info("Black Bars Adjust Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), BlackBarsAdjustInstructionsScansResult[BlackBars2] - (std::uint8_t*)exeModule);

			spdlog::info("Black Bars Adjust Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), BlackBarsAdjustInstructionsScansResult[BlackBars3] - (std::uint8_t*)exeModule);

			spdlog::info("Black Bars Adjust Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), BlackBarsAdjustInstructionsScansResult[BlackBars4] - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(BlackBarsAdjustInstructionsScansResult, BlackBars1, BlackBars4, 0, 6);
			
			BlackBarsAdjustInstruction1Hook = safetyhook::create_mid(BlackBarsAdjustInstructionsScansResult[BlackBars1], [](SafetyHookContext& ctx)
			{
				float& fCurrentBlackBarsValue1 = Memory::ReadMem(ctx.esi + 0x610);

				if (fCurrentBlackBarsValue1 == 0.9999999404f)
				{
					fNewBlackBarsValue1 = fCurrentBlackBarsValue1 / fAspectRatioScale;
				}
				else
				{
					fNewBlackBarsValue1 = fCurrentBlackBarsValue1;
				}

				FPU::FDIV(fNewBlackBarsValue1);
			});

			BlackBarsAdjustInstruction2Hook = safetyhook::create_mid(BlackBarsAdjustInstructionsScansResult[BlackBars2], BlackBarsAdjustInstructionsMidHook);

			BlackBarsAdjustInstruction3Hook = safetyhook::create_mid(BlackBarsAdjustInstructionsScansResult[BlackBars3], BlackBarsAdjustInstructionsMidHook);

			BlackBarsAdjustInstruction4Hook = safetyhook::create_mid(BlackBarsAdjustInstructionsScansResult[BlackBars4], BlackBarsAdjustInstructionsMidHook);
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
