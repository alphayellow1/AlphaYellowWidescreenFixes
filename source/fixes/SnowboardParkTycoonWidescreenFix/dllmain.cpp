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
std::string sFixName = "SnowboardParkTycoonWidescreenFix";
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
float fAspectRatioScale;
float fNewAspectRatio;
float fNewGroundCameraFOV;
float fNewOverviewCameraFOV1;
float fNewOverviewCameraFOV2;
float fNewOverviewCameraFOV3;
float fNewOverviewCameraFOV4;

// Game detection
enum class Game
{
	SPT,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SPT, {"Snowboard Park Tycoon", "SnowBoard_Core.exe"}},
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

static SafetyHookMid GroundCameraAspectRatioInstructionHook{};

void GroundCameraAspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid GroundCameraFOVInstructionHook{};

void GroundCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentGroundCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x4);
	
	fNewGroundCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentGroundCameraFOV, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewGroundCameraFOV]
	}
}

static SafetyHookMid OverviewCameraAspectRatioInstructionHook{};

void OverviewCameraAspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid OverviewCameraFOVInstruction1Hook{};

void OverviewCameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentOverviewCameraFOV1 = *reinterpret_cast<float*>(ctx.esi + 0x44);

	fNewOverviewCameraFOV1 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV1, fAspectRatioScale) * fFOVFactor;
	
	_asm
	{
		fdiv dword ptr ds:[fNewOverviewCameraFOV1]
	}
}

static SafetyHookMid OverviewCameraFOVInstruction2Hook{};

void OverviewCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentOverviewCameraFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

	fNewOverviewCameraFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV2, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewOverviewCameraFOV2]
	}
}

static SafetyHookMid OverviewCameraFOVInstruction4Hook{};

void OverviewCameraFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	float& fCurrentOverviewCameraFOV4 = *reinterpret_cast<float*>(ctx.edx + 0x44);

	fNewOverviewCameraFOV4 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV4, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fdiv dword ptr ds:[fNewOverviewCameraFOV4]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::SPT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* MainMenuResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "C7 44 24 5C 80 02 00 00 C7 44 24 60 E0 01 00 00");
		if (MainMenuResolutionInstructionsScanResult)
		{
			spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), MainMenuResolutionInstructionsScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(MainMenuResolutionInstructionsScanResult + 4, iCurrentResX);
			
			Memory::Write(MainMenuResolutionInstructionsScanResult + 12, iCurrentResY);

			int iNewBitDepth = 32;

			Memory::Write(MainMenuResolutionInstructionsScanResult + 20, iNewBitDepth);
		}
		else
		{
			spdlog::error("Failed to locate main menu resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 40 28 00 04 00 00 E8 B0 69 FE FF C7 40 2C 00 03 00 00 C2 04 00 E8 A1 69 FE FF C7 40 28 20 03 00 00 E8 95 69 FE FF C7 40 2C 58 02 00 00 C2 04 00 E8 86 69 FE FF C7 40 28 80 02 00 00 E8 7A 69 FE FF C7 40 2C E0 01 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 15, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 30, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 42, iCurrentResY);

			// 640x480
			Memory::Write(ResolutionListScanResult + 57, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 69, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		std::uint8_t* GroundCameraAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 4E 3C D9 5A 14 D9 52 28 D8 4E 30 C7 42 2C 00 00 80 3F");
		if (GroundCameraAspectRatioInstructionScanResult)
		{
			spdlog::info("Ground Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), GroundCameraAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(GroundCameraAspectRatioInstructionScanResult, "\x90\x90\x90", 3);

			GroundCameraAspectRatioInstructionHook = safetyhook::create_mid(GroundCameraAspectRatioInstructionScanResult, GroundCameraAspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate ground camera aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* GroundCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 04 8B 44 24 04 D8 0D 0C 76 64 00 89 41 38 D9 C0 D9 F2");
		if (GroundCameraFOVInstructionScanResult)
		{
			spdlog::info("Ground Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GroundCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(GroundCameraFOVInstructionScanResult, "\x90\x90\x90\x90", 4);

			GroundCameraFOVInstructionHook = safetyhook::create_mid(GroundCameraFOVInstructionScanResult, GroundCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate ground camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* OverviewCameraAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 4E 3C 8B 4C 24 20 89 4C 24 14 8D 4C 24 14 51 D9 5C 24 28 D9 46 34 8B 54 24 28 D8 66 30 53");
		if (OverviewCameraAspectRatioInstructionScanResult)
		{
			spdlog::info("Overview Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(OverviewCameraAspectRatioInstructionScanResult, "\x90\x90\x90", 3);

			OverviewCameraAspectRatioInstructionHook = safetyhook::create_mid(OverviewCameraAspectRatioInstructionScanResult, OverviewCameraAspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate overview camera aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "C7 44 24 18 00 00 00 00 E8 3A 03 00 00 D9 05 38 77 64 00 D8 76 44");
		if (OverviewCameraFOVInstruction1ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction1ScanResult + 19 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(OverviewCameraFOVInstruction1ScanResult + 19, "\x90\x90\x90", 3);

			OverviewCameraFOVInstruction1Hook = safetyhook::create_mid(OverviewCameraFOVInstruction1ScanResult + 19, OverviewCameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 41 44 D8 0D 0C 76 64 00 D9 5C 24 00 D9 41 54 D8 2D 40 75 64 00");
		if (OverviewCameraFOVInstruction2ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(OverviewCameraFOVInstruction2ScanResult, "\x90\x90\x90", 3);

			OverviewCameraFOVInstruction2Hook = safetyhook::create_mid(OverviewCameraFOVInstruction2ScanResult, OverviewCameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "8B 56 44 89 54 24 0C 8B 01 2B F8 8B 49 04 89 7C 24 10 2B D9 89 5C 24 14");
		if (OverviewCameraFOVInstruction3ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(OverviewCameraFOVInstruction3ScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid OverviewCameraFOVInstruction3MidHook{};

			OverviewCameraFOVInstruction3MidHook = safetyhook::create_mid(OverviewCameraFOVInstruction3ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV3 = *reinterpret_cast<float*>(ctx.esi + 0x44);

				fNewOverviewCameraFOV3 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV3, fAspectRatioScale) * fFOVFactor;

				ctx.edx = std::bit_cast<uintptr_t>(fNewOverviewCameraFOV3);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "D8 72 44 D9 95 0A 03 00 00 D9 9D 12 03 00 00 D9 85 0A 03 00 00 8B 8D A0 03 00 00 DC C0");
		if (OverviewCameraFOVInstruction4ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction4ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(OverviewCameraFOVInstruction4ScanResult, "\x90\x90\x90", 3);

			OverviewCameraFOVInstruction4Hook = safetyhook::create_mid(OverviewCameraFOVInstruction4ScanResult, OverviewCameraFOVInstruction4MidHook);
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 4 memory address.");
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