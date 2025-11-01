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
std::string sFixName = "CrazyFrogRacerFOVFix";
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
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewHUDHFOV;
float fNewRaceMapHUDVFOV;

// Game detection
enum class Game
{
	CFR,
	Unknown
};

enum CameraFOVInstructionsIndex
{
	CameraFOV1Scan,
	CameraFOV2Scan
};

enum HUDFOVInstructionsIndex
{
	HUDHFOVScan,
	HUDVFOV1Scan,
	HUDVFOV2Scan,
	HUDVFOV3Scan,
	HUDVFOV4Scan,
	HUDVFOV5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CFR, {"Crazy Frog Racer", "CRAZY.EXE"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid MapRadarVFOVInstruction1Hook{};
static SafetyHookMid MapRadarVFOVInstruction2Hook{};
static SafetyHookMid MapRadarVFOVInstruction3Hook{};
static SafetyHookMid MapRadarVFOVInstruction4Hook{};
static SafetyHookMid MapRadarVFOVInstruction5Hook{};

void HUDVFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	FPU::FMUL(fOldAspectRatio);
}

void HUDVFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	FPU::FDIV(fOldAspectRatio);
}

void FOVFix()
{
	if (eGameType == Game::CFR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? 8B 44 24 ?? D8 0D", "8B 44 24 ?? 50 51");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			fNewCameraFOV1 = 179.5f;

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewCameraFOV1);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90\x90\x90", 4);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esp + 0x10);

				fNewCameraFOV2 = (fCurrentCameraFOV2 / fAspectRatioScale) / fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV2);
			});
		}

		std::vector<std::uint8_t*> HUDFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8D 84 24", "D9 05 ?? ?? ?? ?? 8D 0C CD", "D8 70 ?? E8 ?? ?? ?? ??", "D8 49 ?? 8B 4C 24 ?? 85 C9", "D8 49 ?? 8B 54 24", "D8 48 ? A1 ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(HUDFOVInstructionsScansResult) == true)
		{
			spdlog::info("HUD HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVInstructionsScansResult[HUDHFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVInstructionsScansResult[HUDVFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVInstructionsScansResult[HUDVFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVInstructionsScansResult[HUDVFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVInstructionsScansResult[HUDVFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVInstructionsScansResult[HUDVFOV5Scan] - (std::uint8_t*)exeModule);
			
			fNewHUDHFOV = 1.0f * fAspectRatioScale;

			Memory::Write(HUDFOVInstructionsScansResult[HUDHFOVScan] + 1, fNewHUDHFOV);

			fNewRaceMapHUDVFOV = fOldAspectRatio / fAspectRatioScale;

			Memory::PatchBytes(HUDFOVInstructionsScansResult[HUDVFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			MapRadarVFOVInstruction1Hook = safetyhook::create_mid(HUDFOVInstructionsScansResult[HUDVFOV1Scan], [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewRaceMapHUDVFOV);
			});

			Memory::PatchBytes(HUDFOVInstructionsScansResult[HUDVFOV2Scan], "\x90\x90\x90", 3);

			MapRadarVFOVInstruction2Hook = safetyhook::create_mid(HUDFOVInstructionsScansResult[HUDVFOV2Scan], HUDVFOVInstruction2MidHook);

			Memory::PatchBytes(HUDFOVInstructionsScansResult[HUDVFOV3Scan], "\x90\x90\x90", 3);

			MapRadarVFOVInstruction3Hook = safetyhook::create_mid(HUDFOVInstructionsScansResult[HUDVFOV3Scan], HUDVFOVInstruction1MidHook);
			
			Memory::PatchBytes(HUDFOVInstructionsScansResult[HUDVFOV4Scan], "\x90\x90\x90", 3);

			MapRadarVFOVInstruction4Hook = safetyhook::create_mid(HUDFOVInstructionsScansResult[HUDVFOV4Scan], HUDVFOVInstruction1MidHook);

			Memory::PatchBytes(HUDFOVInstructionsScansResult[HUDVFOV5Scan], "\x90\x90\x90", 3);

			MapRadarVFOVInstruction5Hook = safetyhook::create_mid(HUDFOVInstructionsScansResult[HUDVFOV5Scan], HUDVFOVInstruction1MidHook);
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