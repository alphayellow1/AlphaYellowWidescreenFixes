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
std::string sFixName = "TheHistoryChannelCivilWarSecretMissionsFOVChanger";
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

// Ini variables
bool bFixActive;

// Constants
constexpr float fOriginalCameraFOV = 1.22173059f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fNewCameraFOV;

// Game detection
enum class Game
{
	THCCWSM,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::THCCWSM, {"The History Channel: Civil War - Secret Missions", "cw2.exe"}},
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

	// Check if fix is enabled from ini
	inipp::get_value(ini.sections["FOVChanger"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load settings from ini
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
		}
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}

	while (!dllModule2)
	{
		dllModule2 = GetModuleHandleA("CloakNTEngine.dll");
		spdlog::info("Waiting for CloakNTEngine.dll to load...");
	}

	spdlog::info("Successfully obtained handle for CloakNTEngine.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid CameraFOVInstruction4Hook{};

void CameraFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid ActorInitializeFOVInstructionHook{};

void ActorInitializeFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid WeaponReloadCameraFOVInstructionHook{};

void WeaponReloadCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid WeaponChangeCameraFOVInstructionHook{};

void WeaponChangeCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid WeaponResetCameraFOVInstruction1Hook{};

void WeaponResetCameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid WeaponResetCameraFOVInstruction2Hook{};

void WeaponResetCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid WeaponPickupCameraFOVInstructionHook{};

void WeaponPickupCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

static SafetyHookMid StartingSprintCameraFOVInstructionHook{};

void StartingSprintCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [fNewCameraFOV]
	}
}

void FOVChanger()
{
	if (eGameType == Game::THCCWSM && bFixActive == true)
	{
		fNewCameraFOV = fOriginalCameraFOV * fFOVFactor;

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D9 1C 24 D9 04 24 59 C3 CC CC CC CC CC CC CC");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 6, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule2, "89 86 9C 03 00 00 89 86 1C 05 00 00 8B 44 24 10 D9 54 24 10 89 8E 2C 05 00 00 D9 9E 68 05 00 00 8B 8E 88 01 00 00 D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is CloakNTEngine.dll+{:x}", CameraFOVInstruction2ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult + 38, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 44, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(dllModule2, "33 DB 83 F9 03 0F 8C ?? ?? ?? ?? 83 F9 06 0F 8F ?? ?? ?? ?? 8B 80 D8 00 00 00 D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is CloakNTEngine.dll+{:x}", CameraFOVInstruction3ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult + 26, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult + 32, CameraFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction4ScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? 8B 8E DC 04 00 00");
		if (CameraFOVInstruction4ScanResult)
		{
			spdlog::info("Camera FOV Instruction 4: Address is CloakNTEngine.dll+{:x}", CameraFOVInstruction4ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstruction4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstruction4ScanResult + 6, CameraFOVInstruction4MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* SprintCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 46 34 D9 1C 24 E8 ?? ?? ?? ?? C7 46 08 01 00 00 00");
		if (SprintCameraFOVInstructionScanResult)
		{
			spdlog::info("Sprint Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", SprintCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid SprintCameraFOVInstructionMidHook{};

			SprintCameraFOVInstructionMidHook = safetyhook::create_mid(SprintCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentSprintCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x34);

				fCurrentSprintCameraFOV = 1.16370225125f * fNewCameraFOV;
			});
		}
		else
		{
			spdlog::error("Failed to locate sprint camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* ActorInitializeFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? 89 85 D8 00 00 00 8D 4D 54 D9 98 98 00 00 00 C6 80 04 01 00 00 01");
		if (ActorInitializeFOVInstructionScanResult)
		{
			spdlog::info("Actor Initialize FOV Instruction: Address is CloakNTEngine.dll+{:x}", ActorInitializeFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(ActorInitializeFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			ActorInitializeFOVInstructionHook = safetyhook::create_mid(ActorInitializeFOVInstructionScanResult + 6, ActorInitializeFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate actor initialize FOV instruction memory address.");
			return;
		}

		std::uint8_t* WeaponReloadCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? 89 50 24 8B 14 24 D9 58 34 89 50 28 8B 54 24 04 89 50 2C 8B 54 24 08");
		if (WeaponReloadCameraFOVInstructionScanResult)
		{
			spdlog::info("Weapon Reload Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", WeaponReloadCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(WeaponReloadCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponReloadCameraFOVInstructionHook = safetyhook::create_mid(WeaponReloadCameraFOVInstructionScanResult + 6, WeaponReloadCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon reload camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* WeaponChangeCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? C7 46 08 01 00 00 00");
		if (WeaponChangeCameraFOVInstructionScanResult)
		{
			spdlog::info("Weapon Change Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", WeaponChangeCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(WeaponChangeCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponChangeCameraFOVInstructionHook = safetyhook::create_mid(WeaponChangeCameraFOVInstructionScanResult + 6, WeaponChangeCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon change camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* WeaponResetCameraFOVInstruction1ScanResult = Memory::PatternScan(dllModule2, "83 F9 03 0F 8C B0 00 00 00 83 F9 06 0F 8F A7 00 00 00 8B 80 D8 00 00 00 D9 05 ?? ?? ?? ??");
		if (WeaponResetCameraFOVInstruction1ScanResult)
		{
			spdlog::info("Weapon Reset Camera FOV Instruction 1: Address is CloakNTEngine.dll+{:x}", WeaponResetCameraFOVInstruction1ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(WeaponResetCameraFOVInstruction1ScanResult + 24, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponResetCameraFOVInstruction1Hook = safetyhook::create_mid(WeaponResetCameraFOVInstruction1ScanResult + 30, WeaponResetCameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon reset camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* WeaponResetCameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? 89 96 30 05 00 00 D9 9E 80 01 00 00 8B 96 8C 01 00 00 89 86 34 05 00 00 8B 86 90 01 00 00 89 8E A0 03 00 00 89 8E 40 05 00 00 8B 4C 24 0C 89 96 A4 03 00 00 89 96 44 05 00 00 8B 54 24 10 33 DB");
		if (WeaponResetCameraFOVInstruction2ScanResult)
		{
			spdlog::info("Weapon Reset Camera FOV Instruction 2: Address is CloakNTEngine.dll+{:x}", WeaponResetCameraFOVInstruction2ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(WeaponResetCameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponResetCameraFOVInstruction2Hook = safetyhook::create_mid(WeaponResetCameraFOVInstruction2ScanResult + 6, WeaponResetCameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon reset camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* WeaponPickupCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? C7 46 08 01 00 00 00 EB 07 C7 46 08 00 00 00 00 83 7E 08 01 75 3C 8B 4E 04");
		if (WeaponPickupCameraFOVInstructionScanResult)
		{
			spdlog::info("Weapon Pickup Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", WeaponPickupCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(WeaponPickupCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponPickupCameraFOVInstructionHook = safetyhook::create_mid(WeaponPickupCameraFOVInstructionScanResult + 6, WeaponPickupCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon pickup camera FOV instruction memory address.");
			return;
		}

	std:uint8_t* StartingSprintCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? 8B 54 24 20 D9 96 80 01 00 00 89 86 74 01 00 00 D9 9E 84 01 00 00 89 8E 78 01 00 00 89 96 7C 01 00 00 6A 4C D9 54 24 1C 8B 44 24 1C D9 54 24 20 8B 4C 24 20 89 86 88 01 00 00");
		if (StartingSprintCameraFOVInstructionScanResult)
		{
			spdlog::info("Starting Sprint Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", StartingSprintCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(StartingSprintCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			StartingSprintCameraFOVInstructionHook = safetyhook::create_mid(StartingSprintCameraFOVInstructionScanResult + 6, StartingSprintCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate starting sprint camera FOV instruction memory address.");
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
		FOVChanger();
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