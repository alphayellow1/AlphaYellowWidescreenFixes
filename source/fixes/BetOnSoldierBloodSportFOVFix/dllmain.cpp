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
HMODULE dllModule2 = nullptr;
HMODULE dllModule3 = nullptr;
HMODULE dllModule4 = nullptr;

// Fix details
std::string sFixName = "BetOnSoldierBloodSportFOVFix";
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
float fStandardCameraFOVFactor;
float fSuitCameraFOVFactor;
float fStandardZoomFOVFactor;
float fShieldZoomFOVFactor;
float fWeaponFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewHipfireCameraFOV;
float fNewWeaponHipfireFOV;
float fNewShieldZoomFOV;
float fNewStandardZoomFOV;
uint8_t* HipfireCameraFOVAddress;
uint8_t* ShieldZoomFOVAddress;
uint8_t* StandardZoomFOVAddress;

// Game detection
enum class Game
{
	BOSBS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BOSBS, {"Bet on Soldier: Blood Sport", "BoS.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load resolution from ini
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "StandardCameraFOVFactor", fStandardCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "SuitCameraFOVFactor", fSuitCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "StandardZoomFOVFactor", fStandardZoomFOVFactor);
	inipp::get_value(ini.sections["Settings"], "ShieldZoomFOVFactor", fShieldZoomFOVFactor);
	inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", fWeaponFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fStandardCameraFOVFactor);
	spdlog_confparse(fSuitCameraFOVFactor);
	spdlog_confparse(fStandardZoomFOVFactor);
	spdlog_confparse(fShieldZoomFOVFactor);
	spdlog_confparse(fWeaponFOVFactor);

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

	while ((dllModule2 = GetModuleHandleA("kte_core.dll")) == nullptr)
	{
		spdlog::warn("kte_core.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for kte_core.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while ((dllModule3 = GetModuleHandleA("Bos.dll")) == nullptr)
	{
		spdlog::warn("Bos.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for Bos.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	while ((dllModule4 = GetModuleHandleA("kte_dx9.dll")) == nullptr)
	{
		spdlog::warn("kte_dx9.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for kte_dx9.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule4));

	return true;
}

static SafetyHookMid AspectRatioInstruction2Hook{};

void AspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fdiv dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.ebp + 0xD4);

	fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale);

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(ctx.esi + 0xD4);

	fNewCameraFOV3 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV3, fAspectRatioScale);

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV3]
	}
}

static SafetyHookMid WeaponHipfireFOVInstructionHook{};

void WeaponHipfireFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponHipfireFOV = *reinterpret_cast<float*>(ctx.ecx + 0x3C);

	fNewWeaponHipfireFOV = fCurrentWeaponHipfireFOV * fWeaponFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewWeaponHipfireFOV]
	}
}

void FOVFix()
{
	if ((eGameType == Game::BOSBS) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstruction1ScanResult = Memory::PatternScan(dllModule4, "8B 8D ?? ?? ?? ?? 8B 95 ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? 51 8B 8D ?? ?? ?? ?? 52 50 51 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 13 8D 44 24 ?? 50 8B CB FF 92 ?? ?? ?? ?? 83 BD ?? ?? ?? ?? ?? 75 ?? 8B 13 68 ?? ?? ?? ?? 8B CB FF 92 ?? ?? ?? ?? 8B F8 85 FF 74 ?? 8B 0D ?? ?? ?? ?? 8B 01 68 ?? ?? ?? ?? FF 50 ?? 8B F0 85 F6 74 ?? 8B 16 8B CE FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? 8B CE 89 B5 ?? ?? ?? ?? 8B 11 57 FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 0F 84 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B F8 8D 83 ?? ?? ?? ?? 50 8D 8B ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? 51 8B CE 89 7C 24 ?? FF 15 ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D 54 24 ?? F3 ?? 8B 8B ?? ?? ?? ?? 52 FF 15 ?? ?? ?? ?? 8B 54 24 ?? 8B F0 8D 7A ?? B9 ?? ?? ?? ?? F3 ?? 8D BA ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? F3 ?? 8B 8D ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 48 ?? 85 C9 DB 40 ?? 7D ?? D8 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? D9 C0 D9 98 ?? ?? ?? ?? D9 85 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D8 F9 D9 98 ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? 8B 11 DD D8 FF 92 ?? ?? ?? ?? 8B 03 8B CB FF 90 ?? ?? ?? ?? 8B 08 8D 94 24 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 45 ?? 8D B5 ?? ?? ?? ?? 56 8B CD FF 90 ?? ?? ?? ?? 8B 13 56 8B CB FF 92 ?? ?? ?? ?? 80 7C 24 ?? ?? 74 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 81 EC");
		if (AspectRatioInstruction1ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is kte_dx9.dll+{:x}", AspectRatioInstruction1ScanResult - (std::uint8_t*)dllModule4);

			Memory::PatchBytes(AspectRatioInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid AspectRatioInstruction1MidHook{};

			AspectRatioInstruction1MidHook = safetyhook::create_mid(AspectRatioInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(dllModule2, "D8 B6 ?? ?? ?? ?? D9 E8");
		if (AspectRatioInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 2: Address is kte_core.dll+{:x}", AspectRatioInstruction2ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstruction2ScanResult, AspectRatioInstruction2MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction 2 memory address.");
			return;
		}

		// 

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(dllModule4, "8B 8D ?? ?? ?? ?? 52 50 51 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 13 8D 44 24 ?? 50 8B CB FF 92 ?? ?? ?? ?? 83 BD ?? ?? ?? ?? ?? 75 ?? 8B 13 68 ?? ?? ?? ?? 8B CB FF 92 ?? ?? ?? ?? 8B F8 85 FF 74 ?? 8B 0D ?? ?? ?? ?? 8B 01 68 ?? ?? ?? ?? FF 50 ?? 8B F0 85 F6 74 ?? 8B 16 8B CE FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? 8B CE 89 B5 ?? ?? ?? ?? 8B 11 57 FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 0F 84 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B F8 8D 83 ?? ?? ?? ?? 50 8D 8B ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? 51 8B CE 89 7C 24 ?? FF 15 ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D 54 24 ?? F3 ?? 8B 8B ?? ?? ?? ?? 52 FF 15 ?? ?? ?? ?? 8B 54 24 ?? 8B F0 8D 7A ?? B9 ?? ?? ?? ?? F3 ?? 8D BA ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? F3 ?? 8B 8D ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 48 ?? 85 C9 DB 40 ?? 7D ?? D8 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? D9 C0 D9 98 ?? ?? ?? ?? D9 85 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D8 F9 D9 98 ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? 8B 11 DD D8 FF 92 ?? ?? ?? ?? 8B 03 8B CB FF 90 ?? ?? ?? ?? 8B 08 8D 94 24 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 45 ?? 8D B5 ?? ?? ?? ?? 56 8B CD FF 90 ?? ?? ?? ?? 8B 13 56 8B CB FF 92 ?? ?? ?? ?? 80 7C 24 ?? ?? 74 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 81 EC");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is kte_dx9.dll+{:x}", CameraFOVInstruction1ScanResult - (std::uint8_t*)dllModule4);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction1MidHook{};

			CameraFOVInstruction1MidHook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.ebp + 0xD4);

				fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, fAspectRatioScale);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV1);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule4, "D9 85 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D8 F9 D9 98 ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? 8B 11 DD D8 FF 92 ?? ?? ?? ?? 8B 03 8B CB FF 90 ?? ?? ?? ?? 8B 08 8D 94 24 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 45 ?? 8D B5 ?? ?? ?? ?? 56 8B CD FF 90 ?? ?? ?? ?? 8B 13 56 8B CB FF 92 ?? ?? ?? ?? 80 7C 24 ?? ?? 74 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 81 EC");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is kte_dx9.dll+{:x}", CameraFOVInstruction2ScanResult - (std::uint8_t*)dllModule4);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(dllModule2, "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8D 47");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is kte_core.dll+{:x}", CameraFOVInstruction3ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult, CameraFOVInstruction3MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* HipfireCameraFOVInstructionScanResult = Memory::PatternScan(dllModule3, "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 53 56 8B F1 8B 0D ?? ?? ?? ??");
		if (HipfireCameraFOVInstructionScanResult)
		{
			spdlog::info("Hipfire Camera/Shield Zoom/Standard Zoom FOV Instructions Scan: Address is Bos.dll+{:x}", HipfireCameraFOVInstructionScanResult - (std::uint8_t*)dllModule3);

			HipfireCameraFOVAddress = Memory::GetPointer<uint32_t>(HipfireCameraFOVInstructionScanResult + 1, Memory::PointerMode::Absolute);

			ShieldZoomFOVAddress = Memory::GetPointer<uint32_t>(HipfireCameraFOVInstructionScanResult + 7, Memory::PointerMode::Absolute);

			StandardZoomFOVAddress = Memory::GetPointer<uint32_t>(HipfireCameraFOVInstructionScanResult + 17, Memory::PointerMode::Absolute);

			Memory::PatchBytes(HipfireCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid HipfireCameraAndShieldZoomFOVInstructionsMidHook{};

			HipfireCameraAndShieldZoomFOVInstructionsMidHook = safetyhook::create_mid(HipfireCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireCameraFOV = *reinterpret_cast<float*>(HipfireCameraFOVAddress);

				fNewHipfireCameraFOV = fCurrentHipfireCameraFOV * fStandardCameraFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewHipfireCameraFOV);

				float& fCurrentShieldZoomFOV = *reinterpret_cast<float*>(ShieldZoomFOVAddress);

				fNewShieldZoomFOV = fCurrentShieldZoomFOV / fShieldZoomFOVFactor;

				ctx.edx = std::bit_cast<uintptr_t>(fNewShieldZoomFOV);
			});

			Memory::PatchBytes(HipfireCameraFOVInstructionScanResult + 15, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid StandardCameraZoomFOVInstructionMidHook{};

			StandardCameraZoomFOVInstructionMidHook = safetyhook::create_mid(HipfireCameraFOVInstructionScanResult + 15, [](SafetyHookContext& ctx)
			{
				float& fCurrentStandardZoomFOV = *reinterpret_cast<float*>(StandardZoomFOVAddress);

				fNewStandardZoomFOV = fCurrentStandardZoomFOV / fStandardZoomFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewStandardZoomFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the hipfire camera, shield camera zoom and standard zoom FOV instructions scan memory address.");
			return;
		}

		std::uint8_t* WeaponFOVInstructionScanResult = Memory::PatternScan(dllModule3, "D9 41 40 C2 04 00 D9 41 3C");
		if (WeaponFOVInstructionScanResult)
		{
			spdlog::info("Weapon FOV Instruction: Address is Bos.dll+{:x}", WeaponFOVInstructionScanResult - (std::uint8_t*)dllModule3);

			Memory::PatchBytes(WeaponFOVInstructionScanResult + 6, "\x90\x90\x90", 3);

			WeaponHipfireFOVInstructionHook = safetyhook::create_mid(WeaponFOVInstructionScanResult + 6, WeaponHipfireFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the weapon FOV instruction memory address.");
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
		FOVFix();
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