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
HMODULE thisModule;
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "ConstantineWidescreenFix";
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
constexpr float fOriginalAspectRatio = 0.75f;
constexpr float fOriginalCameraFOV = 0.5f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewAspectRatio2;
float fFOVFactor;
float fNewCameraFOV;
int iOldHeight;
float fMultiplierValue;
int iHUDOffset;
int iHUDWidth;

// Game detection
enum class Game
{
	CONSTANTINE,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CONSTANTINE, {"Constantine", "Constantine.exe"}},
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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	float fValue1 = 1.0f;
	float fValue2 = 0.9375f;
	float fValue3 = 0.625f;

	_asm
	{
		mov edx, dword ptr ds : [ecx + 0xE9F48]
		fild dword ptr ds : [ecx + 0xE9F44]
		fidiv dword ptr ds : [ecx + 0xE9F48]
		fstp dword ptr ds : [fNewAspectRatio]
		fld dword ptr ds : [fNewAspectRatio]
		fdiv dword ptr ds : [fOldAspectRatio]
		fstp dword ptr ds : [fNewAspectRatio2]
		fild dword ptr ds : [iOldHeight]
		fmul dword ptr ds : [fNewAspectRatio2]
		fistp dword ptr ds : [iHUDWidth]
		fild dword ptr ds : [iHUDWidth]
		fisub dword ptr ds : [iOldHeight]
		fmul dword ptr ds : [fMultiplierValue]
		fistp dword ptr ds : [iHUDOffset]
		cmp dword ptr ds : [fNewAspectRatio] , 0x3FE38E39
		jg firstvalue
		jmp Continuecode

		firstvalue :
		mov dword ptr ds : [0x007B2610] , 0x3F520000
			mov dword ptr ds : [0x007B2618] , 0x3DF00000
			jmp Continuecode2

			Continuecode :
		fld dword ptr ds : [fNewAspectRatio2]
			fsub dword ptr ds : [fValue1]
			fmul dword ptr ds : [fMultiplierValue]
			fadd dword ptr ds : [fValue1]
			fstp dword ptr ds : [0x007B2620]
			fld dword ptr ds : [fValue2]
			fdiv dword ptr ds : [0x007B2620]
			fstp dword ptr ds : [0x007B2610]
			fld dword ptr ds : [fValue2]
			fsub dword ptr ds : [0x007B2610]
			fstp dword ptr ds : [0x007B2618]

			Continuecode2 :
			cmp dword ptr ds : [fNewAspectRatio] , 0x3FE38E39
			jl secondvalue
			jmp Continuecode3

			secondvalue :
		mov dword ptr ds : [0x007B2614] , 0x3F200000
			mov dword ptr ds : [0x007B261C] , 0
			jmp BackToOriginalCode

			Continuecode3 :
		fld dword ptr ds : [fNewAspectRatio2]
			fdiv dword ptr ds : [fOldAspectRatio]
			fsub dword ptr ds : [fValue1]
			fmul dword ptr ds : [fMultiplierValue]
			fadd dword ptr ds : [fValue1]
			fstp dword ptr ds : [0x007B2624]
			fld dword ptr ds : [fValue3]
			fmul dword ptr ds : [0x007B2624]
			fstp dword ptr ds : [0x007B2614]
			fld dword ptr ds : [fValue3]
			fsub dword ptr ds : [0x007B2614]
			fstp dword ptr ds : [0x007B261C]

			BackToOriginalCode :
			mov dword ptr ss : [esp + 0xC] , edx
	}
}

static SafetyHookMid FMVAspectRatioInstructionHook{};

void FMVAspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		push dword ptr ds : [0x007B2610]
		push dword ptr ds : [0x007B2614]
		push dword ptr ds : [0x007B2618]
		push dword ptr ds : [0x007B261C]
	}
}

static SafetyHookMid InterfaceScalePrimaryInstructionHook{};

void InterfaceScalePrimaryInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [esi + 0xEA048]
		fmul dword ptr ds : [fNewAspectRatio2]
		fstp dword ptr ds : [esi + 0xEA048]
	}
}

static SafetyHookMid InterfaceScaleSecondaryInstructionHook{};

void InterfaceScaleSecondaryInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		mov ecx, dword ptr ds : [iHUDWidth]
		mov dword ptr ds : [edi + 0xE9900] , ecx
		mov ecx, 0
		cvtsi2ss xmm1, dword ptr ds : [edi + 0xE9900]
		mov dword ptr ds : [edi + 0xE9900] , 0x280
	}
}

static SafetyHookMid MoveInterfaceInstructionHook{};

void MoveInterfaceInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		mov ecx, dword ptr ds : [iHUDOffset]
		add dword ptr ss : [esp + 0x78] , ecx
		mov ecx, 0
		cvtsi2ss xmm0, dword ptr ss : [esp + 0x78]
	}
}

static SafetyHookMid HealthBarScaleInstructionHook{};

void HealthBarScaleInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [edi + 0xEA048]
		fmul dword ptr ds : [fNewAspectRatio2]
		fstp dword ptr ds : [edi + 0xEA048]
	}
}

static SafetyHookMid TextScaleInstructionHook{};

void TextScaleInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		mov edx, dword ptr ds : [iHUDWidth]
		mov dword ptr ds : [edi + 0xE9900] , edx
		mov edx, 0x4
		cvtsi2ss xmm7, dword ptr ds : [edi + 0xE9900]
		mov dword ptr ds : [edi + 0xE9900] , 0x280
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fdiv dword ptr ds : [fNewAspectRatio2]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::CONSTANTINE && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewAspectRatio2 = fNewAspectRatio / fOldAspectRatio;

		iOldHeight = 640;

		fMultiplierValue = 0.5f;

		iHUDOffset = 1;

		iHUDWidth = 100;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 91 48 9F 0E 00 89 54 24 0C 0F 57 C0 8B 49 04 F3 0F 11 44 24 10");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult + 10, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* FMVAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "68 00 00 70 3F 68 00 00 20 3F 6A 00 6A 00 8B CE");
		if (FMVAspectRatioInstructionScanResult)
		{
			spdlog::info("FMV Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), FMVAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(FMVAspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 14);

			FMVAspectRatioInstructionHook = safetyhook::create_mid(FMVAspectRatioInstructionScanResult + 14, FMVAspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate FMV aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* InterfaceScalePrimaryInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 10 96 48 A0 0E 00 F3 0F 2A AE 04 99 0E 00 F3 0F 10 25 E8 E2 60 00 F3 0F 10 5C 24 1C");
		if (InterfaceScalePrimaryInstructionScanResult)
		{
			spdlog::info("Interface Scale Primary Instruction: Address is {:s}+{:x}", sExeName.c_str(), InterfaceScalePrimaryInstructionScanResult - (std::uint8_t*)exeModule);

			InterfaceScalePrimaryInstructionHook = safetyhook::create_mid(InterfaceScalePrimaryInstructionScanResult, InterfaceScalePrimaryInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate interface scale primary instruction memory address.");
			return;
		}

		std::uint8_t* InterfaceScaleSecondaryInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 2A 8F 00 99 0E 00 F3 0F 10 15 6C E5 60 00 F3 0F 2A 44 24 78");
		if (InterfaceScaleSecondaryInstructionScanResult)
		{
			spdlog::info("Interface Scale Secondary Instruction: Address is {:s}+{:x}", sExeName.c_str(), InterfaceScaleSecondaryInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(InterfaceScaleSecondaryInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			InterfaceScaleSecondaryInstructionHook = safetyhook::create_mid(InterfaceScaleSecondaryInstructionScanResult + 8, InterfaceScaleSecondaryInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate interface scale secondary instruction memory address.");
			return;
		}

		std::uint8_t* MoveInterfaceInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 2A 44 24 78 F3 0F 10 A7 48 A0 0E 00 F3 0F 10 35 E8 E2 60 00");
		if (MoveInterfaceInstructionScanResult)
		{
			spdlog::info("Move Interface Instruction: Address is {:s}+{:x}", sExeName.c_str(), MoveInterfaceInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(MoveInterfaceInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			MoveInterfaceInstructionHook = safetyhook::create_mid(MoveInterfaceInstructionScanResult + 6, MoveInterfaceInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate move interface instruction memory address.");
			return;
		}

		std::uint8_t* HealthBarScaleInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 10 9F 48 A0 0E 00 F3 0F 2A BF 04 99 0E 00 F3 0F 10 2D E8 E2 60 00 8B 87 98 69 0F 00");
		if (HealthBarScaleInstructionScanResult)
		{
			spdlog::info("Health Bar Scale Instruction: Address is {:s}+{:x}", sExeName.c_str(), HealthBarScaleInstructionScanResult - (std::uint8_t*)exeModule);

			HealthBarScaleInstructionHook = safetyhook::create_mid(HealthBarScaleInstructionScanResult, HealthBarScaleInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate health bar scale instruction memory address.");
			return;
		}

		std::uint8_t* TextScaleInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 2A BF 00 99 0E 00 DD D8 F3 0F 11 74 24 10 F3 0F 5C 4C 24 10");
		if (TextScaleInstructionScanResult)
		{
			spdlog::info("Text Scale Instruction: Address is {:s}+{:x}", sExeName.c_str(), TextScaleInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(TextScaleInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			TextScaleInstructionHook = safetyhook::create_mid(TextScaleInstructionScanResult + 8, TextScaleInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate text scale instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 56 30 D9 17 D8 4E 24 D9 9E 94 00 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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