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
std::string sFixName = "SecretServiceFOVChanger";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fZoomFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
uintptr_t HipfireFOVAddress;
uintptr_t SprintFOVAddress;
uintptr_t ChangeWeaponFOV1Address;
uintptr_t ChangeWeaponFOV2Address;
uintptr_t ChangeWeaponFOV3Address;
uintptr_t WeaponShowAddress;

// Game detection
enum class Game
{
	SS,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	Hipfire,
	Zoom,
	Sprint,
	ChangeWeapon1,
	ChangeWeapon2,
	ChangeWeapon3,
	WeaponShow
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SS, {"Secret Service", "ss.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "ZoomFactor", fZoomFactor);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fZoomFactor);

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

	dllModule2 = Memory::GetHandle("CloakNTEngine.dll");

	return true;
}

static SafetyHookMid HipfireFOVInstructionHook{};
static SafetyHookMid ZoomFOVInstructionHook{};
static SafetyHookMid SprintFOVInstructionHook{};
static SafetyHookMid ChangeWeaponFOVInstruction1Hook{};
static SafetyHookMid ChangeWeaponFOVInstruction2Hook{};
static SafetyHookMid ChangeWeaponFOVInstruction3Hook{};
static SafetyHookMid WeaponShowFOVInstructionHook{};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float fovFactor)
{
	float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

	fNewCameraFOV = fCurrentCameraFOV * fovFactor;

	FPU::FLD(fNewCameraFOV);
}

void FOVChanger()
{
	if (eGameType == Game::SS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D9 1C 24 D9 04 24 59 C3 CC", "D9 46 ?? D9 1C 24 E8 ?? ?? ?? ?? D9 EE D8 5C 24",
		"D9 05 ?? ?? ?? ?? DC 05 ?? ?? ?? ?? C7 06 ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? D9 5E ?? C6 46 ?? ?? 8B C6", "D9 05 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? EB ?? 8B 46",
		"DB 83 F9 ?? 0F 8C ?? ?? ?? ?? 83 F9 ?? 0F 8F ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? D9 05 ?? ?? ?? ??", "D9 05 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 8B 4C 24 ?? 89 96 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? 8B 54 24 ?? 89 86",
		"D9 05 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? 8B 8E");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Hipfire FOV Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionsScansResult[Hipfire] - (std::uint8_t*)dllModule2);

			spdlog::info("Zoom FOV Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionsScansResult[Zoom] - (std::uint8_t*)dllModule2);

			spdlog::info("Sprint FOV Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionsScansResult[Sprint] - (std::uint8_t*)dllModule2);

			spdlog::info("Change Weapon FOV Instruction 1: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionsScansResult[ChangeWeapon1] - (std::uint8_t*)dllModule2);

			spdlog::info("Change Weapon FOV Instruction 2: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionsScansResult[ChangeWeapon2] + 25 - (std::uint8_t*)dllModule2);

			spdlog::info("Change Weapon FOV Instruction 3: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionsScansResult[ChangeWeapon3] - (std::uint8_t*)dllModule2);

			spdlog::info("Weapon Show FOV Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionsScansResult[WeaponShow] - (std::uint8_t*)dllModule2);

			HipfireFOVAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[Hipfire] + 2, Memory::PointerMode::Absolute);

			SprintFOVAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[Sprint] + 2, Memory::PointerMode::Absolute);

			ChangeWeaponFOV1Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[ChangeWeapon1] + 2, Memory::PointerMode::Absolute);

			ChangeWeaponFOV2Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[ChangeWeapon2] + 27, Memory::PointerMode::Absolute);

			ChangeWeaponFOV3Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[ChangeWeapon3] + 2, Memory::PointerMode::Absolute);

			WeaponShowAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[WeaponShow] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Hipfire], 6);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Zoom], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult, Sprint, ChangeWeapon1, 0, 6);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[ChangeWeapon2] + 25, 6);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult, ChangeWeapon3, WeaponShow, 0, 6);

			HipfireFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Hipfire], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(HipfireFOVAddress, fFOVFactor);
			});

			ZoomFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x34, 1.0f / fZoomFactor);
			});

			SprintFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Sprint], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(SprintFOVAddress, fFOVFactor);
			});

			ChangeWeaponFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ChangeWeapon1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ChangeWeaponFOV1Address, fFOVFactor);
			});

			ChangeWeaponFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ChangeWeapon2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ChangeWeaponFOV2Address, fFOVFactor);
			});

			ChangeWeaponFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ChangeWeapon3], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ChangeWeaponFOV3Address, fFOVFactor);
			});

			WeaponShowFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[WeaponShow], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(WeaponShowAddress, fFOVFactor);
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