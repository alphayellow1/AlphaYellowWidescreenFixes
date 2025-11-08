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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fNewCameraFOV;
float fNewAspectRatio;
float fAspectRatioScale;
float fNewSprintCameraFOV;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
float fNewActorInitializeFOV;
float fNewWeaponReloadCameraFOV;
float fNewWeaponChangeCameraFOV;
float fNewWeaponResetCameraFOV1;
float fNewWeaponResetCameraFOV2;
float fNewWeaponPickupCameraFOV;
float fNewStartingSprintCameraFOV;
float fCurrentStartingSprintCameraFOV;
float fCurrentSprintCameraFOV;
uint8_t* CameraFOV1Address;
uint8_t* CameraFOV2Address;
uint8_t* CameraFOV3Address;
uint8_t* CameraFOV4Address;
uint8_t* ActorInitializeFOVAddress;
uint8_t* WeaponReloadCameraFOVAddress;
uint8_t* WeaponChangeCameraFOVAddress;
uint8_t* WeaponResetCamera1FOVAddress;
uint8_t* WeaponResetCamera2FOVAddress;
uint8_t* WeaponPickupCameraFOVAddress;
uint8_t* StartingSprintCameraFOVAddress;

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
	bool bGameFound = false;

	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			bGameFound = true;
			break;
		}
	}

	if (bGameFound == false)
	{
		spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		return false;
	}

	while ((dllModule2 = GetModuleHandleA("CloakNTEngine.dll")) == nullptr)
	{
		spdlog::warn("CloakNTEngine.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for CloakNTEngine.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraFOVInstruction1Hook{};

void CameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(CameraFOV1Address);

	fNewCameraFOV1 = fCurrentCameraFOV1 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV1]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(CameraFOV2Address);

	fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(CameraFOV3Address);

	fNewCameraFOV3 = fCurrentCameraFOV3 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV3]
	}
}

static SafetyHookMid CameraFOVInstruction4Hook{};

void CameraFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV4 = *reinterpret_cast<float*>(CameraFOV4Address);

	fNewCameraFOV4 = fCurrentCameraFOV4 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV4]
	}
}

static SafetyHookMid ActorInitializeFOVInstructionHook{};

void ActorInitializeFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentActorInitializeFOV = *reinterpret_cast<float*>(ActorInitializeFOVAddress);

	fNewActorInitializeFOV = fCurrentActorInitializeFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewActorInitializeFOV]
	}
}

static SafetyHookMid WeaponReloadCameraFOVInstructionHook{};

void WeaponReloadCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponReloadCameraFOV = *reinterpret_cast<float*>(WeaponReloadCameraFOVAddress);

	fNewWeaponReloadCameraFOV = fCurrentWeaponReloadCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewWeaponReloadCameraFOV]
	}
}

static SafetyHookMid WeaponChangeCameraFOVInstructionHook{};

void WeaponChangeCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponChangeCameraFOV = *reinterpret_cast<float*>(WeaponChangeCameraFOVAddress);

	fNewWeaponChangeCameraFOV = fCurrentWeaponChangeCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewWeaponChangeCameraFOV]
	}
}

static SafetyHookMid WeaponResetCameraFOVInstruction1Hook{};

void WeaponResetCameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponResetCameraFOV1 = *reinterpret_cast<float*>(WeaponResetCamera1FOVAddress);

	fNewWeaponResetCameraFOV1 = fCurrentWeaponResetCameraFOV1 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewWeaponResetCameraFOV1]
	}
}

static SafetyHookMid WeaponResetCameraFOVInstruction2Hook{};

void WeaponResetCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponResetCameraFOV2 = *reinterpret_cast<float*>(WeaponResetCamera2FOVAddress);

	fNewWeaponResetCameraFOV2 = fCurrentWeaponResetCameraFOV2 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewWeaponResetCameraFOV2]
	}
}

static SafetyHookMid WeaponPickupCameraFOVInstructionHook{};

void WeaponPickupCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponPickupCameraFOV = *reinterpret_cast<float*>(WeaponPickupCameraFOVAddress);

	fNewWeaponPickupCameraFOV = fCurrentWeaponPickupCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewWeaponPickupCameraFOV]
	}
}

static SafetyHookMid StartingSprintCameraFOVInstructionHook{};

void StartingSprintCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentStartingSprintCameraFOV = *reinterpret_cast<float*>(StartingSprintCameraFOVAddress);

	fNewStartingSprintCameraFOV = fCurrentStartingSprintCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewStartingSprintCameraFOV]
	}
}

static SafetyHookMid SprintCameraFOVInstructionHook{};

void SprintCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentSprintCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x34);

	fNewSprintCameraFOV = fCurrentSprintCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewSprintCameraFOV]
	}
}

void FOVChanger()
{
	if (eGameType == Game::THCCWSM && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D9 1C 24 D9 04 24 59 C3 CC CC CC CC CC CC CC");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is CloakNTEngine.dll+{:x}", CameraFOVInstruction1ScanResult - (std::uint8_t*)dllModule2);

			CameraFOV1Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction1ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, CameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule2, "89 86 9C 03 00 00 89 86 1C 05 00 00 8B 44 24 10 D9 54 24 10 89 8E 2C 05 00 00 D9 9E 68 05 00 00 8B 8E 88 01 00 00 D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is CloakNTEngine.dll+{:x}", CameraFOVInstruction2ScanResult - (std::uint8_t*)dllModule2);

			CameraFOV2Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction2ScanResult + 40, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult + 38, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 38, CameraFOVInstruction2MidHook);
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

			CameraFOV3Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction3ScanResult + 28, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult + 26, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult + 26, CameraFOVInstruction3MidHook);
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

			CameraFOV4Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction4ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstruction4ScanResult, CameraFOVInstruction4MidHook);
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

			Memory::PatchBytes(SprintCameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			SprintCameraFOVInstructionHook = safetyhook::create_mid(SprintCameraFOVInstructionScanResult, SprintCameraFOVInstructionMidHook);			
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

			ActorInitializeFOVAddress = Memory::GetPointerFromAddress<uint32_t>(ActorInitializeFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ActorInitializeFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			ActorInitializeFOVInstructionHook = safetyhook::create_mid(ActorInitializeFOVInstructionScanResult, ActorInitializeFOVInstructionMidHook);
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

			WeaponReloadCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(WeaponReloadCameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponReloadCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponReloadCameraFOVInstructionHook = safetyhook::create_mid(WeaponReloadCameraFOVInstructionScanResult, WeaponReloadCameraFOVInstructionMidHook);
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

			WeaponChangeCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(WeaponChangeCameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponChangeCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponChangeCameraFOVInstructionHook = safetyhook::create_mid(WeaponChangeCameraFOVInstructionScanResult, WeaponChangeCameraFOVInstructionMidHook);
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

			WeaponResetCamera1FOVAddress = Memory::GetPointerFromAddress<uint32_t>(WeaponResetCameraFOVInstruction1ScanResult + 26, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponResetCameraFOVInstruction1ScanResult + 24, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponResetCameraFOVInstruction1Hook = safetyhook::create_mid(WeaponResetCameraFOVInstruction1ScanResult + 24, WeaponResetCameraFOVInstruction1MidHook);
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

			WeaponResetCamera2FOVAddress = Memory::GetPointerFromAddress<uint32_t>(WeaponResetCameraFOVInstruction2ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponResetCameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponResetCameraFOVInstruction2Hook = safetyhook::create_mid(WeaponResetCameraFOVInstruction2ScanResult, WeaponResetCameraFOVInstruction1MidHook);
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

			WeaponPickupCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(WeaponPickupCameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponPickupCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponPickupCameraFOVInstructionHook = safetyhook::create_mid(WeaponPickupCameraFOVInstructionScanResult, WeaponPickupCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon pickup camera FOV instruction memory address.");
			return;
		}

	    std::uint8_t* StartingSprintCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? 8B 54 24 20 D9 96 80 01 00 00 89 86 74 01 00 00 D9 9E 84 01 00 00 89 8E 78 01 00 00 89 96 7C 01 00 00 6A 4C D9 54 24 1C 8B 44 24 1C D9 54 24 20 8B 4C 24 20 89 86 88 01 00 00");
		if (StartingSprintCameraFOVInstructionScanResult)
		{
			spdlog::info("Starting Sprint Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", StartingSprintCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			StartingSprintCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(StartingSprintCameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(StartingSprintCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			StartingSprintCameraFOVInstructionHook = safetyhook::create_mid(StartingSprintCameraFOVInstructionScanResult, StartingSprintCameraFOVInstructionMidHook);
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