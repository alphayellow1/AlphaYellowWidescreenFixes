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

// Ini variables
bool bFixActive;

// Variables
float fFOVFactor;
float fNewCameraFOV;

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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		mov edx, dword ptr ds : [ecx + 0xE9F48]
		fild dword ptr ds : [ecx + 0xE9F44]
		fidiv dword ptr ds : [ecx + 0xE9F48]
		fstp dword ptr ds : [0x007B2600]
		fld dword ptr ds : [0x007B2600]
		fdiv dword ptr ds : [0x0060E5F4]
		fstp dword ptr ds : [0x007B2604]
		fild dword ptr ds : [0x005C5895]
		fmul dword ptr ds : [0x007B2604]
		fistp dword ptr ds : [0x007B2608]
		fild dword ptr ds : [0x007B2608]
		fisub dword ptr ds : [0x005C5895]
		fmul dword ptr ds : [0x0060E2E8]
		fistp dword ptr ds : [0x007B260C]
		cmp dword ptr ds : [0x007B2600] , 0x3FE38E39
		jg firstvalue
		jmp continuecode

		firstvalue :
		mov dword ptr ds : [0x007B2610] , 0x3F520000
			mov dword ptr ds : [0x007B2618] , 0x3DF00000
			jmp continuecode2

			continuecode :
		fld dword ptr ds : [0x007B2604]
			fsub dword ptr ds : [0x0060E2E4]
			fmul dword ptr ds : [0x0060E2E8]
			fadd dword ptr ds : [0x0060E2E4]
			fstp dword ptr ds : [0x007B2620]
			fld dword ptr ds : [0x006879A4]
			fdiv dword ptr ds : [0x007B2620]
			fstp dword ptr ds : [0x007B2610]
			fld dword ptr ds : [0x006879A4]
			fsub dword ptr ds : [0x007B2610]
			fstp dword ptr ds : [0x007B2618]

			continuecode2 :
			cmp dword ptr ds : [0x007B2600] , 0x3FE38E39
			jl value2
			jmp Continuecode3

			value2 :
		mov dword ptr ds : [0x007B2614] , 0x3F200000
			mov dword ptr ds : [0x007B261C] , 0
			jmp backtooriginalcode

			continuecode3 :
		fld dword ptr ds : [0x007B2604]
			fdiv dword ptr ds : [0x0060E5F4]
			fsub dword ptr ds : [0x0060E2E4]
			fmul dword ptr ds : [0x0060E2E8]
			fadd dword ptr ds : [0x0060E2E4]
			fstp dword ptr ds : [0x007B2624]
			fld dword ptr ds : [0x0061A5D8]
			fmul dword ptr ds : [0x007B2624]
			fstp dword ptr ds : [0x007B2614]
			fld dword ptr ds : [0x0061A5D8]
			fsub dword ptr ds : [0x007B2614]
			fstp dword ptr ds : [0x007B261C]

			backtooriginalcode :
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
		fmul dword ptr ds : [0x007B2604]
		fstp dword ptr ds : [esi + 0xEA048]
	}
}

static SafetyHookMid InterfaceScaleSecondaryInstructionHook{};

void InterfaceScaleSecondaryInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		mov ecx, dword ptr ds : [0x007B2608]
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
		mov ecx, dword ptr ds : [0x007B260C]
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
		fmul dword ptr ds : [0x007B2604]
		fstp dword ptr ds : [edi + 0xEA048]
	}
}

static SafetyHookMid TextScaleInstructionHook{};

void TextScaleInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		mov edx, dword ptr ds : [0x007B2608]
		mov dword ptr ds : [edi + 0xE9900] , edx
		mov edx, 0x4
		cvtsi2ss xmm7, dword ptr ds : [edi + 0xE9900]
		mov dword ptr ds : [edi + 0xE9900] , 0x280
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fNewCameraFOV = 1.0f / fFOVFactor;

	_asm
	{
		fdiv dword ptr ds : [0x007B2604]
		fmul dword ptr ds : [fNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::CONSTANTINE && bFixActive == true)
	{
		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 91 48 9F 0E 00 89 54 24 0C 0F 57 C0 8B 49 04 F3 0F 11 44 24 10");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 10);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, AspectRatioInstructionMidHook);
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

			Memory::WriteNOPs(FMVAspectRatioInstructionScanResult, 14);

			FMVAspectRatioInstructionHook = safetyhook::create_mid(FMVAspectRatioInstructionScanResult, FMVAspectRatioInstructionMidHook);
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

			Memory::WriteNOPs(InterfaceScaleSecondaryInstructionScanResult, 8);

			InterfaceScaleSecondaryInstructionHook = safetyhook::create_mid(InterfaceScaleSecondaryInstructionScanResult, InterfaceScaleSecondaryInstructionMidHook);
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

			Memory::WriteNOPs(MoveInterfaceInstructionScanResult, 6);

			MoveInterfaceInstructionHook = safetyhook::create_mid(MoveInterfaceInstructionScanResult, MoveInterfaceInstructionMidHook);
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

			Memory::WriteNOPs(TextScaleInstructionScanResult, 8);

			TextScaleInstructionHook = safetyhook::create_mid(TextScaleInstructionScanResult, TextScaleInstructionMidHook);
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