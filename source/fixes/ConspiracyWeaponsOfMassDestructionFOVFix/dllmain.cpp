// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>
#include <vector>
#include <algorithm>
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

// Fix details
std::string sFixName = "ConspiracyWeaponsOfMassDestructionFOVFix";
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
constexpr float fTolerance = 0.0000001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fAspectRatioScale;
float fNewCameraFOV;
static uint8_t* PistolHipfireCameraFOVValueAddress;
static uint8_t* M4HipfireCameraFOVValueAddress;
static uint8_t* SniperHipfireCameraFOVValueAddress;
float fNewPistolHipfireCameraFOV;
float fNewM4HipfireCameraFOV;
float fNewSniperHipfireCameraFOV;

// Game detection
enum class Game
{
	CWOMD,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CWOMD, {"Conspiracy: Weapons of Mass Destruction", "cwmd.exe"}},
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

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * atanf(tanf(fCurrentFOV / 2.0f) * fAspectRatioScale);
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ebx + 0x24);

	// Computes the new FOV value
	if (fCurrentCameraFOV != 1.134464025f && fCurrentCameraFOV != CalculateNewFOV(1.134464025f) * fFOVFactor)
	{
		fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV);
	}
	else
	{
		fNewCameraFOV = fCurrentCameraFOV;
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::CWOMD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioScanResult = Memory::PatternScan(exeModule, "AB AA AA 3F 63 6F 6E 74 72 61 69 6C");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScanResult - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScanResult, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 43 24 33 C0 D8 0D ?? ?? ?? ?? 55 8D AB CC 00 00 00 56 D9 F2");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* PistolHipfireCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 89 08 83 A6 2C 01 00 00 EF DD D8 5F 5E C3 D9 86 64 02 00 00");
		if (PistolHipfireCameraFOVInstructionScanResult)
		{
			spdlog::info("Pistol Hipfire Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), PistolHipfireCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			uint32_t imm1 = *reinterpret_cast<uint32_t*>(PistolHipfireCameraFOVInstructionScanResult + 2);

			PistolHipfireCameraFOVValueAddress = reinterpret_cast<uint8_t*>(imm1);
			
			Memory::PatchBytes(PistolHipfireCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid PistolHipfireCameraFOVInstructionMidHook{};
			
			PistolHipfireCameraFOVInstructionMidHook = safetyhook::create_mid(PistolHipfireCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentPistolHipfireCameraFOV = *reinterpret_cast<float*>(PistolHipfireCameraFOVValueAddress);

				fNewPistolHipfireCameraFOV = CalculateNewFOV(fCurrentPistolHipfireCameraFOV) * fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewPistolHipfireCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate pistol hipfire camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* M4HipfireCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 89 08 83 A6 2C 01 00 00 EF DD D8 5F 5E C3 D9 86 80 02 00 00 D8 1D ?? ?? ?? ??");
		if (M4HipfireCameraFOVInstructionScanResult)
		{
			spdlog::info("M4 Hipfire Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), M4HipfireCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			uint32_t imm2 = *reinterpret_cast<uint32_t*>(M4HipfireCameraFOVInstructionScanResult + 2);

			M4HipfireCameraFOVValueAddress = reinterpret_cast<uint8_t*>(imm2);

			Memory::PatchBytes(M4HipfireCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid M4HipfireCameraFOVInstructionMidHook{};

			M4HipfireCameraFOVInstructionMidHook = safetyhook::create_mid(M4HipfireCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentM4HipfireCameraFOV = *reinterpret_cast<float*>(M4HipfireCameraFOVValueAddress);

				fNewM4HipfireCameraFOV = CalculateNewFOV(fCurrentM4HipfireCameraFOV) * fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewM4HipfireCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate M4 hipfire camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* SniperHipfireCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 89 08 74 17 B8 00 00 80 3F 89 87 B0 01 00 00 89 87 B4 01 00 00 89 85 F4 00 00 00");
		if (SniperHipfireCameraFOVInstructionScanResult)
		{
			spdlog::info("Sniper Hipfire Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), SniperHipfireCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			uint32_t imm3 = *reinterpret_cast<uint32_t*>(SniperHipfireCameraFOVInstructionScanResult + 2);

			SniperHipfireCameraFOVValueAddress = reinterpret_cast<uint8_t*>(imm3);
			
			Memory::PatchBytes(SniperHipfireCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid SniperHipfireCameraFOVInstructionMidHook{};
			
			SniperHipfireCameraFOVInstructionMidHook = safetyhook::create_mid(SniperHipfireCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentSniperHipfireCameraFOV = *reinterpret_cast<float*>(SniperHipfireCameraFOVValueAddress);

				fNewSniperHipfireCameraFOV = CalculateNewFOV(fCurrentSniperHipfireCameraFOV) * fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewSniperHipfireCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate sniper hipfire camera FOV instruction memory address.");
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