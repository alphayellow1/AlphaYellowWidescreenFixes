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
#include <algorithm>
#include <cmath>
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "RugbyLeagueWidescreenFix";
std::string sFixVersion = "1.1";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHUDAspectRatio;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fNewCameraFOV2;

// Game detection
enum class Game
{
	RL,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResolutionInstruction1,
	ResolutionInstruction2,
	ResolutionInstruction3,
	ResolutionInstruction4,
	ResolutionInstruction5,
	ResolutionInstruction6,
	GameplayResolutionInstruction1,
	GameplayResolutionInstruction2
};

enum AspectRatioInstructionsIndices
{
	HUDARScan,
	IntroCamARScan,
	GameplayCam1ARScan
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RL, {"Rugby League", "RugbyLeague.exe"}},
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

static SafetyHookMid GameplayResolutionWidthInstruction1Hook{};
static SafetyHookMid GameplayResolutionHeightInstruction1Hook{};
static SafetyHookMid GameplayResolutionWidthInstruction2Hook{};
static SafetyHookMid GameplayResolutionHeightInstruction2Hook{};
static SafetyHookMid CameraFOVInstructions1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid IntroCamAspectRatioInstructionHook{};
static SafetyHookMid GameplayCam1AspectRatioInstructionHook{};

void AspectRatioInstructionsMidHook(SafetyHookContext& ctx)
{
	FPU::FMUL(fNewAspectRatio);
}

void WidescreenFix()
{
	if (eGameType == Game::RL && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "6A 20 68 E0 01 00 00 68 80 02 00 00 C7 86 0C 01 00", "FF F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D",
		"E8 26 F9 F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D 44 24 20 81", "CF F1 F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D 4C", "6A 20 BF E0 01 00 00 57 BE 80 02 00 00 56 E8 A8 85 0F", "C8 7D 49 BE 80 02 00 00 BF E0 01 00 00 51 8D 4C 24",
		"89 0D C0 09 71 00 8B 57 04 89 15 C4 09 71 00 C7 05 D4 09 71 00 02 00 00 00", "8B 48 60 DB 41 0C 8B 51 10 89 54 24 14");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{			
			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction1] + 8, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction1] + 3, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction2] + 9, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction2] + 4, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction3] + 11, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction3] + 6, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction4] + 10, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction4] + 5, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction5] + 9, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction5] + 3, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction6] + 4, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionInstruction6] + 9, iCurrentResY);			

			GameplayResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
			
			GameplayResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction1] + 9, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});			

			GameplayResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction2] + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x10) = iCurrentResY;
			});			

			GameplayResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[GameplayResolutionInstruction2] + 3, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0xC) = iCurrentResX;
			});
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE", "D8 0D ?? ?? ?? ?? 8B 44 24 ?? 50", "D8 0D ?? ?? ?? ?? 51 51 8B 4C 24");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUDARScan] - (std::uint8_t*)exeModule);

			spdlog::info("Intro Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[IntroCamARScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayCam1ARScan] - (std::uint8_t*)exeModule);

			fNewHUDAspectRatio = 0.5f * fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[HUDARScan] + 1, fNewHUDAspectRatio);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[IntroCamARScan], 6);

			IntroCamAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[IntroCamARScan], AspectRatioInstructionsMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayCam1ARScan], 6);

			GameplayCam1AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayCam1ARScan], AspectRatioInstructionsMidHook);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 4C 24 ?? 8B 44 24 ?? 8B D1 89 4C 24", "D9 44 24 ?? D8 74 24 ?? 6A ?? D9 5C 24 ?? D9 44 24 ?? 8B 4C 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1Scan], 8);

			CameraFOVInstructions1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.esp + 0x38);

				if (fCurrentCameraHFOV != fNewCameraHFOV)
				{
					fNewCameraHFOV = fCurrentCameraHFOV * fAspectRatioScale * fFOVFactor;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraHFOV);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.esp + 0x3C);

				if (fCurrentCameraVFOV != fNewCameraVFOV)
				{
					fNewCameraVFOV = fNewCameraHFOV / fNewAspectRatio;
				}
				
				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2Scan], 4);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esp + 0x40);

				fNewCameraFOV2 = fCurrentCameraFOV2 / fFOVFactor;

				FPU::FLD(fNewCameraFOV2);
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