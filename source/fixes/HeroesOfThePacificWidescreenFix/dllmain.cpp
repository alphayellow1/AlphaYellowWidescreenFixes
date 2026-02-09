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
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "HeroesOfThePacificWidescreenFix";
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
float fNewGameplayAspectRatio;
float fNewGameplayCameraFOV;
float fNewBriefingScreenAspectRatio;
double dNewBriefingScreenCameraFOV;
uint8_t* BriefingScreenFOV1Address;
uint8_t* BriefingScreenFOV2Address;

// Game detection
enum class Game
{
	HOTP,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1,
	Res2,
	Res3,
	Res4,
	Res5,
	Res6,
	Res7,
};

enum AspectRatioInstructionsIndices
{
	GameplayAR,
	BriefingAR1,
	BriefingAR2
};

enum CameraFOVInstructionsIndices
{
	GameplayFOV,
	BriefingFOV1,
	BriefingFOV2
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::HOTP, {"Heroes of the Pacific", "Heroes.exe"}},
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

static SafetyHookMid GameplayAspectRatioInstructionHook{};
static SafetyHookMid BriefingScreenAspectRatioInstruction1Hook{};
static SafetyHookMid BriefingScreenAspectRatioInstruction2Hook{};
static SafetyHookMid GameplayCameraFOVInstructionHook{};
static SafetyHookMid BriefingScreenCameraFOVInstruction1Hook{};
static SafetyHookMid BriefingScreenCameraFOVInstruction2Hook{};

void BriefingScreenCameraFOVInstructionsMidHook(uint8_t* FOVAddress)
{
	double& dCurrentBriefingScreenCameraFOV = *reinterpret_cast<double*>(FOVAddress);

	dNewBriefingScreenCameraFOV = Maths::CalculateNewFOV_RadBased(dCurrentBriefingScreenCameraFOV, fAspectRatioScale, Maths::AngleMode::HalfAngle);

	FPU::FLD(dNewBriefingScreenCameraFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::HOTP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "C7 01 ?? ?? ?? ?? C7 03 ?? ?? ?? ?? C7 07", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A", "81 FF ?? ?? ?? ?? 7D", "C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? C3 E9",
		"C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 8B 47", "C7 45 ?? ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? EB ?? C7 05", "C7 46 ?? ?? ?? ?? ?? EB ?? 46");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res5] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res6] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res7] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[Res1] + 2, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1] + 8, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res2] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res2] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res3] + 2, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3] + 10, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3] + 19, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res3] + 27, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 10, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res5] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res5] + 10, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res6] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res6] + 10, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res7] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res7] + 15, iCurrentResY);
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 30 D8 48 ?? D8 F9 D9 1C ?? D8 70 ?? D9 5C 24 ?? 74 ?? 8D 04 ?? 50 51 E8 ?? ?? ?? ?? 83 C4 ?? 83 C4 ?? C2 ?? ?? CC CC CC CC CC", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? 74 ?? 8D 4C 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 83 C4 ?? 5F");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAR] - (std::uint8_t*)exeModule);

			spdlog::info("Briefing Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[BriefingAR1] - (std::uint8_t*)exeModule);

			spdlog::info("Briefing Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[BriefingAR2] - (std::uint8_t*)exeModule);

			fNewGameplayAspectRatio = 1.0f / fAspectRatioScale;

			fNewBriefingScreenAspectRatio = 0.75f / fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[GameplayAR] + 2, &fNewGameplayAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[BriefingAR1] + 2, &fNewBriefingScreenAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[BriefingAR2] + 2, &fNewBriefingScreenAspectRatio);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 40 ?? D8 0D ?? ?? ?? ?? 8B 09 85 C9 5F D9 F2 5E DD D8", "DD 05 ?? ?? ?? ?? A1", "DD 05 ?? ?? ?? ?? D9 F2");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Briefing Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[BriefingFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Briefing Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[BriefingFOV2] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV], 3);

			GameplayCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0x8);

				fNewGameplayCameraFOV = fCurrentGameplayCameraFOV * fFOVFactor;

				FPU::FLD(fNewGameplayCameraFOV);
			});

			BriefingScreenFOV1Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[BriefingFOV1] + 2, Memory::PointerMode::Absolute);

			BriefingScreenFOV2Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[BriefingFOV2] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[BriefingFOV1], 6);

			BriefingScreenCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[BriefingFOV1], [](SafetyHookContext& ctx)
			{
				BriefingScreenCameraFOVInstructionsMidHook(BriefingScreenFOV1Address);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[BriefingFOV2], 6);

			BriefingScreenCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[BriefingFOV2], [](SafetyHookContext& ctx)
			{
				BriefingScreenCameraFOVInstructionsMidHook(BriefingScreenFOV2Address);
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