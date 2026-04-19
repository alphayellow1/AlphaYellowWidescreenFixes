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
std::string sFixName = "BeyondNormandyAssignmentBerlinWidescreenFix";
std::string sFixVersion = "1.4";
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
float fHUDMargin;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fCompassNeedleWidth;
float fLoadingBarX;
float fRedCrossAndMenuTextWidth;
float fHUDLeftMargin;
float fHUDWidth;
float fCompassNeedleX;
float fCompassX;
float fCompassWidth;
float fWeaponsListWidth;
float fAmmoX;
float fAmmoWidth;
float fUnknownHUDElement1;
double dObjectiveDirectionWidth;
double dHUDTextWidth;

// Game detection
enum class Game
{
	BNAB,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResList1,
	ResList2
};

enum HUDElementsIndices
{
	HUD1,
	HUD2,
	HUD3,
	HUD4,
	HUD5,
	HUD6,
	HUD7,
	HUD8,
	HUD9,
	HUD10,
	HUD11,
	HUD12,
	HUD13
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BNAB, {"Beyond Normandy: Assignment Berlin", "AssignmentBerlin-V1.05.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "HUDMargin", fHUDMargin);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fHUDMargin);

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

void WidescreenFix()
{
	if (eGameType == Game::BNAB && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 50",
		"C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 B4 24");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResList1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResList2] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[ResList1] + 4, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResList1] + 12, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResList2] + 4, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResList2] + 12, iCurrentResY);
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? 74 ?? D9 86");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV = 0.8f * fAspectRatioScale * fFOVFactor;

			Memory::Write(CameraFOVInstructionScanResult + 4, fNewCameraFOV);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> HUDInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? D9 44 24",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 8B C8", "D8 0D ?? ?? ?? ?? D8 44 24 ?? D9 5C 24 ?? D9 44 24",
		"D8 0D ?? ?? ?? ?? 51 8D 4C 24 ?? D9 1C 24", "DC 0D ?? ?? ?? ?? D9 C9 D9 C9 DE C9 D8 0D ?? ?? ?? ?? D8 44 24", "D8 0D ?? ?? ?? ?? 50 51 8D 4C 24 ?? D9 5C 24 ?? 8B 44 24",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? 8B 4C 24 ?? D8 0D ?? ?? ?? ?? D9 54 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24 ?? D8 0D ?? ?? ?? ?? D8 2D",
		"DC 0D ?? ?? ?? ?? D9 58 ?? D9 44 24 ?? DC 0D ?? ?? ?? ?? D9 58", "D8 0D ?? ?? ?? ?? 50 51 8D 4C 24 ?? D9 5C 24 ?? D9 44 24",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? 8B 4C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24", "D8 0D ?? ?? ?? ?? 8B 44 24 ?? 50", "D8 0D ?? ?? ?? ?? D8 44 24 ?? D9 5C 24 ?? D9 44 24");
		if (Memory::AreAllSignaturesValid(HUDInstructionsScansResult) == true)
		{
			spdlog::info("(HUD) Loading Bar X Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD1] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Red Cross and Menu Text Width Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD2] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Left Margin: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD3] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass Needle X: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD4] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Objective Direction Width: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD5] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass X: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD6] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass Width: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD7] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Weapons List Width: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD8] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Text Width: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD9] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Ammo X: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD10] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Ammo Width: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD11] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass Needle Width: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD12] - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Unknown Element 1 Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD13] - (std::uint8_t*)exeModule);

			static uintptr_t LoadingBarXAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD1] + 2, Memory::PointerMode::Absolute);
			static uintptr_t RedCrossAndMenuTextWidthAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD2] + 2, Memory::PointerMode::Absolute);
			static uintptr_t LeftMarginAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD3] + 2, Memory::PointerMode::Absolute);
			static uintptr_t CompassNeedleXAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD4] + 2, Memory::PointerMode::Absolute);
			static uintptr_t ObjectiveDirectionWidthAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD5] + 2, Memory::PointerMode::Absolute);
			static uintptr_t CompassXAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD6] + 2, Memory::PointerMode::Absolute);
			static uintptr_t CompassWidthAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD7] + 2, Memory::PointerMode::Absolute);
			static uintptr_t WeaponsListWidthAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD8] + 2, Memory::PointerMode::Absolute);
			static uintptr_t TextWidthAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD9] + 2, Memory::PointerMode::Absolute);
			static uintptr_t AmmoXAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD10] + 2, Memory::PointerMode::Absolute);
			static uintptr_t AmmoWidthAddress = Memory::GetPointerFromAddress(HUDInstructionsScansResult[HUD11] + 2, Memory::PointerMode::Absolute);

			fHUDWidth = 600.0f * fNewAspectRatio;

			// Loading bar X
			fLoadingBarX = 222.0f + (fHUDWidth - 800.0f) / 2.0f;

			Memory::Write(LoadingBarXAddress, fLoadingBarX);

			// Red cross and menu text width
			fRedCrossAndMenuTextWidth = 0.001666f / fNewAspectRatio;

			Memory::Write(RedCrossAndMenuTextWidthAddress, fRedCrossAndMenuTextWidth);

			// HUD left margin
			fHUDLeftMargin = 0.0333333f / fNewAspectRatio + fHUDMargin / fHUDWidth;

			Memory::Write(LeftMarginAddress, fHUDLeftMargin);

			// Compass needle X
			fCompassNeedleX = (fHUDWidth - 79.0f - fHUDMargin) / fHUDWidth;

			Memory::Write(CompassNeedleXAddress, fCompassNeedleX);

			// Objective direction width
			dObjectiveDirectionWidth = 0.001666 / static_cast<double>(fNewAspectRatio);

			Memory::Write(ObjectiveDirectionWidthAddress, dObjectiveDirectionWidth);

			// Compass X
			fCompassX = (fHUDWidth - 118.0f - fHUDMargin) / fHUDWidth;

			Memory::Write(CompassXAddress, fCompassX);

			// Compass width
			fCompassWidth = 0.163333f / fNewAspectRatio;

			Memory::Write(CompassWidthAddress, fCompassWidth);

			// Weapons list width
			fWeaponsListWidth = 0.17f / fNewAspectRatio;

			Memory::Write(WeaponsListWidthAddress, fWeaponsListWidth);

			// HUD text width
			dHUDTextWidth = 0.0006666666666 / static_cast<double>(fNewAspectRatio);

			Memory::Write(TextWidthAddress, dHUDTextWidth);

			// Ammo X
			fAmmoX = (fHUDWidth - 74.0f - fHUDMargin) / fHUDWidth;

			Memory::Write(AmmoXAddress, fAmmoX);

			// Ammo width
			fAmmoWidth = 0.116666f / fNewAspectRatio;

			Memory::Write(AmmoWidthAddress, fAmmoWidth);

			// Compass needle width
			fCompassNeedleWidth = 0.0333333f / fNewAspectRatio;

			Memory::Write(HUDInstructionsScansResult[HUD12] + 2, &fCompassNeedleWidth);

			// Unknown HUD element #1
			fUnknownHUDElement1 = 0.025000000372529f;

			Memory::Write(HUDInstructionsScansResult[HUD13] + 2, &fUnknownHUDElement1);
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