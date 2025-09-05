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
float fAspectRatioScale;
float fNewGameplayAspectRatio;
float fNewGameplayCameraFOV;
float fNewBriefingScreenAspectRatio;
double dNewBriefingScreenCameraFOV1;
double dNewBriefingScreenCameraFOV2;
uint8_t* BriefingScreenCameraFOV1Address;
uint8_t* BriefingScreenCameraFOV2Address;

// Game detection
enum class Game
{
	HOTP,
	Unknown
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

void GameplayAspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewGameplayAspectRatio]
	}
}

static SafetyHookMid GameplayCameraFOVInstructionHook{};

void GameplayCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentGameplayCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0x8);

	fNewGameplayCameraFOV = fCurrentGameplayCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewGameplayCameraFOV]
	}
}

static SafetyHookMid BriefingScreenAspectRatioInstruction1Hook{};

void BriefingScreenAspectRatioInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewBriefingScreenAspectRatio]
	}
}

static SafetyHookMid BriefingScreenCameraFOVInstruction1Hook{};

void BriefingScreenCameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	double& dCurrentBriefingScreenCameraFOV1 = *reinterpret_cast<double*>(BriefingScreenCameraFOV1Address);

	dNewBriefingScreenCameraFOV1 = Maths::CalculateNewFOV_RadBased(dCurrentBriefingScreenCameraFOV1, fAspectRatioScale, Maths::AngleMode::HalfAngle);

	_asm
	{
		fld qword ptr ds:[dNewBriefingScreenCameraFOV1]
	}
}

static SafetyHookMid BriefingScreenAspectRatioInstruction2Hook{};

void BriefingScreenAspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewBriefingScreenAspectRatio]
	}
}

static SafetyHookMid BriefingScreenCameraFOVInstruction2Hook{};

void BriefingScreenCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	double& dCurrentBriefingScreenCameraFOV2 = *reinterpret_cast<double*>(BriefingScreenCameraFOV2Address);

	dNewBriefingScreenCameraFOV2 = Maths::CalculateNewFOV_RadBased(dCurrentBriefingScreenCameraFOV2, fAspectRatioScale, Maths::AngleMode::HalfAngle);

	_asm
	{
		fld qword ptr ds:[dNewBriefingScreenCameraFOV2]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::HOTP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
		
		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "C7 01 20 03 00 00 C7 03 58 02 00 00");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions1ScanResult + 2, iCurrentResX);

			Memory::Write(ResolutionInstructions1ScanResult + 8, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "68 58 02 00 00 68 20 03 00 00 6A 00 68");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions2ScanResult + 6, iCurrentResX);

			Memory::Write(ResolutionInstructions2ScanResult + 1, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "CE 81 FF 20 03 00 00 7D 09 81 C2 20 03 00 00 89 50 08 81 F9 58 02 00 00 7D 59 81 C6 58 02 00 00");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions3ScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionInstructions3ScanResult + 11, iCurrentResX);

			Memory::Write(ResolutionInstructions3ScanResult + 20, iCurrentResY);

			Memory::Write(ResolutionInstructions3ScanResult + 28, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "C7 41 04 20 03 00 00 C7 41 08 58 02 00 00");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions4ScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionInstructions4ScanResult + 10, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 4 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "C7 46 40 20 03 00 00 C7 46 44 58 02 00 00");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions5ScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionInstructions5ScanResult + 10, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "C7 45 F4 20 03 00 00 C7 45 F8 58 02 00 00");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions6ScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionInstructions6ScanResult + 10, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 6 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions7ScanResult = Memory::PatternScan(exeModule, "C7 46 24 20 03 00 00 EB 03 46 73 48 C7 46 28 58 02 00 00");
		if (ResolutionInstructions7ScanResult)
		{
			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions7ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions7ScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionInstructions7ScanResult + 15, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 7 scan memory address.");
			return;
		}

		std::uint8_t* GameplayAspectRatioInstruction1ScanResult = Memory::PatternScan(exeModule, "5E DD D8 D9 05 ?? ?? ?? ?? D8 30 D8 48 0C D8 F9 D9 1C 24 D8 70 10 D9 5C 24 04 74 0D 8D 04 24 50 51");
		if (GameplayAspectRatioInstruction1ScanResult)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayAspectRatioInstruction1ScanResult + 3 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(GameplayAspectRatioInstruction1ScanResult + 3, "\x90\x90\x90\x90\x90\x90", 6);

			fNewGameplayAspectRatio = 1.0f / fAspectRatioScale;

			GameplayAspectRatioInstructionHook = safetyhook::create_mid(GameplayAspectRatioInstruction1ScanResult + 3, GameplayAspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate gameplay aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* GameplayCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 40 08 D8 0D ?? ?? ?? ?? 8B 09 85 C9 5F D9 F2 5E DD D8");
		if (GameplayCameraFOVInstructionScanResult)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(GameplayCameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			GameplayCameraFOVInstructionHook = safetyhook::create_mid(GameplayCameraFOVInstructionScanResult, GameplayCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate gameplay camera FOV instruction memory address.");
			return;
		}

		fNewBriefingScreenAspectRatio = 0.75f / fAspectRatioScale;

		std::uint8_t* BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? A1 ?? ?? ?? ?? D9 F2 8B 00 85 C0 DD D8 D9 54 24 54 D8 0D ?? ?? ?? ??");
		if (BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult)
		{
			spdlog::info("Briefing Screen Aspect Ratio & Camera FOV Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult + 23, "\x90\x90\x90\x90\x90\x90", 6);			

			BriefingScreenAspectRatioInstruction1Hook = safetyhook::create_mid(BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult + 23, BriefingScreenAspectRatioInstruction1MidHook);

			BriefingScreenCameraFOV1Address = Memory::GetPointer<uint32_t>(BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			BriefingScreenCameraFOVInstruction1Hook = safetyhook::create_mid(BriefingScreenAspectRatioAndCameraFOVInstructions1ScanResult, BriefingScreenCameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate briefing aspect ratio & camera FOV instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? D9 F2 8B 0E 8D 44 24 28 50 51 DD D8 D9 54 24 30 D8 0D ?? ?? ?? ??");
		if (BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult)
		{
			spdlog::info("Briefing Screen Aspect Ratio & Camera FOV Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult + 22, "\x90\x90\x90\x90\x90\x90", 6);

			BriefingScreenAspectRatioInstruction2Hook = safetyhook::create_mid(BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult + 22, BriefingScreenAspectRatioInstruction2MidHook);

			BriefingScreenCameraFOV2Address = Memory::GetPointer<uint32_t>(BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			BriefingScreenCameraFOVInstruction2Hook = safetyhook::create_mid(BriefingScreenAspectRatioAndCameraFOVInstructions2ScanResult, BriefingScreenCameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate briefing aspect ratio & camera FOV instructions 2 scan memory address.");
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