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
std::string sFixName = "BetOnSoldierBlackOutSaigonFOVFix";
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
	BOSBOS,
	BOSBOSGAME,
	Unknown
};

enum CameraFOVInstructionsScansIndices
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan,
	HipfireAndZoomFOVScan,
	WeaponFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BOSBOS, {"Bet on Soldier: Black-out Saigon", "BoS.exe"}},
	{Game::BOSBOSGAME, {"Bet on Soldier: Black-out Saigon", "BetOnSoldier_BlackOutSaigon.exe"}},
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

	dllModule2 = Memory::GetHandle("kte_core.dll");

	dllModule3 = Memory::GetHandle("Bos.dll");

	dllModule4 = Memory::GetHandle("kte_dx9.dll");

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid HipfireCameraAndShieldZoomFOVInstructionsHook{};
static SafetyHookMid StandardCameraZoomFOVInstructionHook{};
static SafetyHookMid WeaponHipfireFOVInstructionHook{};

void FOVFix()
{
	if ((eGameType == Game::BOSBOS || eGameType == Game::BOSBOSGAME) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "D8 B6 E0 00 00 00 D9 E8 D9 F3 D9 C0 D9 FF D9 5C 24 0C");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is kte_core.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);			

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.esi + 0xE0);

				if (fCurrentAspectRatio == 1.3333333333f)
				{
					fCurrentAspectRatio = fNewAspectRatio;
				}
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule4, "8B 8E ?? ?? ?? ?? 52 50 51 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 55 ?? 8D 44 24 ?? 50 8B CD FF 92 ?? ?? ?? ?? 83 BE ?? ?? ?? ?? ?? 75 ?? 8B 55 ?? 68 ?? ?? ?? ?? 8B CD FF 92 ?? ?? ?? ?? 8B D8 85 DB 74 ?? 8B 0D ?? ?? ?? ?? 8B 01 68 ?? ?? ?? ?? FF 50 ?? 8B F8 85 FF 74 ?? 8B 17 8B CF FF 52 ?? 8B 8E ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? 8B CF 89 BE ?? ?? ?? ?? 8B 11 53 FF 52 ?? 8B 8E ?? ?? ?? ?? 85 C9 0F 84 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B D8 8D 85 ?? ?? ?? ?? 50 8D 8D ?? ?? ?? ?? 8D B5 ?? ?? ?? ?? 51 8B CE FF 15 ?? ?? ?? ?? B9 ?? ?? ?? ?? 8B FB 8D 54 24 ?? F3 ?? 8B 8D ?? ?? ?? ?? 52 FF 15 ?? ?? ?? ?? 8B F0 8D 7B ?? B9 ?? ?? ?? ?? F3 ?? 8D BB ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D B5 ?? ?? ?? ?? F3 ?? 8B 74 24 ?? 8B 8E ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 48 ?? 85 C9 DB 40 ?? 7D ?? D8 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B CD", "D9 86 ?? ?? ?? ?? D8 0D", dllModule2, "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8D 58", dllModule3, "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 53 56 8B F1 8B 0D ?? ?? ?? ?? 89 44 24 08 89 4C 24 10 89 54 24 0C 74 1D A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ??", "D9 41 40 C2 04 00 D9 41 3C C2 04 00 CC CC CC CC CC CC CC CC CC CC CC CC CC");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is kte_dx9.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)dllModule4);

			spdlog::info("Camera FOV Instruction 2: Address is kte_dx9.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)dllModule4);

			spdlog::info("Camera FOV Instruction 3: Address is kte_core.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Hipfire & Zoom Camera FOV Instruction: Address is Bos.dll+{:x}", CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan] - (std::uint8_t*)dllModule3);

			spdlog::info("Weapon FOV Instruction: Address is Bos.dll+{:x}", CameraFOVInstructionsScansResult[WeaponFOVScan] - (std::uint8_t*)dllModule3);

			// Camera FOV Instruction 1 Hook
			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);
			
			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.esi + 0xD4);

				fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, fAspectRatioScale);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV1);
			});

			// Camera FOV Instruction 2 Hook
			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esi + 0xD4);

				fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale);

				FPU::FLD(fNewCameraFOV2);
			});

			// Camera FOV Instruction 3 Hook
			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV3Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(ctx.esi + 0xD4);

				fNewCameraFOV3 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV3, fAspectRatioScale);

				FPU::FLD(fNewCameraFOV3);
			});

			// Hipfire & Zoom Camera FOV Instructions Hook
			HipfireCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan] + 1, Memory::PointerMode::Absolute);

			ShieldZoomFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan] + 7, Memory::PointerMode::Absolute);

			StandardZoomFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan] + 17, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			HipfireCameraAndShieldZoomFOVInstructionsHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireCameraFOV = *reinterpret_cast<float*>(HipfireCameraFOVAddress);

				fNewHipfireCameraFOV = fCurrentHipfireCameraFOV * fStandardCameraFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewHipfireCameraFOV);

				float& fCurrentShieldZoomFOV = *reinterpret_cast<float*>(ShieldZoomFOVAddress);

				fNewShieldZoomFOV = fCurrentShieldZoomFOV / fShieldZoomFOVFactor;

				ctx.edx = std::bit_cast<uintptr_t>(fNewShieldZoomFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan] + 15, "\x90\x90\x90\x90\x90\x90", 6);

			StandardCameraZoomFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireAndZoomFOVScan] + 15, [](SafetyHookContext& ctx)
			{
				float& fCurrentStandardZoomFOV = *reinterpret_cast<float*>(StandardZoomFOVAddress);

				fNewStandardZoomFOV = fCurrentStandardZoomFOV / fStandardZoomFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewStandardZoomFOV);
			});

			// Weapon FOV Instruction Hook
			Memory::PatchBytes(CameraFOVInstructionsScansResult[WeaponFOVScan] + 6, "\x90\x90\x90", 3);

			WeaponHipfireFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[WeaponFOVScan] + 6, [](SafetyHookContext& ctx)
			{
				float& fCurrentWeaponHipfireFOV = *reinterpret_cast<float*>(ctx.ecx + 0x3C);

				fNewWeaponHipfireFOV = fCurrentWeaponHipfireFOV * fWeaponFOVFactor;

				FPU::FLD(fNewWeaponHipfireFOV);
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