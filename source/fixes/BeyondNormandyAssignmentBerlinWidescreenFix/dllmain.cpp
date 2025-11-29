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
std::string sFixVersion = "1.3";
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
float fNewCameraHFOV;
float fNewCameraVFOV;
double dObjectiveDirectionWidth;
double dHUDTextWidth;
uint8_t newWidthSmall;
uint8_t newWidthBig;
uint8_t newHeightSmall;
uint8_t newHeightBig;

// Game detection
enum class Game
{
	BNAB,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Resolution1Scan,
	Resolution2Scan
};

enum HUDElementsIndex
{
	HUD1Scan,
	HUD2Scan,
	HUD3Scan,
	HUD4Scan,
	HUD5Scan,
	HUD6Scan
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

static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};
static SafetyHookMid CompassNeedleWidthInstructionHook{};
static SafetyHookMid UnknownHUDElement1InstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::BNAB && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "?? ?? ?? ?? C7 44 24 40 ?? ?? ?? ?? E8 6F 2C FF", "C7 44 24 50 ?? ?? ?? ?? C7 44 24 54 ?? ?? ?? ?? 89 B4 24");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			newWidthSmall = iCurrentResX % 256;

			newWidthBig = (iCurrentResX - newWidthSmall) / 256;

			newHeightSmall = iCurrentResY % 256;

			newHeightBig = (iCurrentResY - newHeightSmall) / 256;

			// Resolution 1 Scan
			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan], iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan] + 8, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan], newWidthSmall);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan] + 1, newWidthBig);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan] + 8, newHeightSmall);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan] + 9, newHeightBig);

			// Resolution 2 Scan
			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 4, newWidthSmall);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 5, newWidthBig);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 12, newHeightSmall);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 13, newHeightBig);
		}

		std::uint8_t* CameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "8B 44 24 0C 56 8B F1 8B 4C 24 14 8B 16 89 4E 0C");
		if (CameraFOVInstructionsScanResult)
		{
			spdlog::info("Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScanResult, "\x90\x90\x90\x90", 4);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.esp + 0xC);

				if (fCurrentCameraHFOV != fNewCameraHFOV)
				{
					fNewCameraHFOV = (fCurrentCameraHFOV * fAspectRatioScale) * fFOVFactor;
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionsScanResult + 7, "\x90\x90\x90\x90", 4);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult + 7, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.esp + 0x14);

				if (fCurrentCameraVFOV != fNewCameraVFOV)
				{
					fNewCameraVFOV = (fCurrentCameraVFOV * fAspectRatioScale) * fFOVFactor;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instructions scan memory address.");
		}

		std::vector<std::uint8_t*> HUDElementsScansResult = Memory::PatternScan(exeModule, "00 00 1C 42 00 80 B1 43 00 00 50 42 00 00 5E 43 0E 74 DA 3A 0A D7 A3 3A 23 20 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 2D 20 25 73 0A 00 00", "00 00 00 89 88 08 3D CD CC CC 3C 89 88 88 3C CD CC 4C 3C AB AA 6A 3F 00 00 80 3E 67 61 6D 65", "DA 0F 49 40 52 B8 66 3F 00 00 D0 41 00 00 F0 40 00 00 00 00 4F 1B E8 B4 81 4E 5B 3F 00 00 5C 42 00 00 00 00 7B 14 AE 47 E1 7A 54 3F 00 00 FA C3 71 3D 5A 3F DA 40 27 3E 48 E1 FA 3D 00 00 48 42 00 00 98 41 00 00 C8 43 5C 8F 02 3E 6F 12 83 3D", "D8 45 3F FC A9 F1 D2 4D 62 40 3F 00 00 11 44 00 80 85 43 00 00 50 41 00 00 C4 43 00 00 30 41 00 00 D1 43 25 64 2F 25 64 00 00 00 3A 6D 60 3F EC 51 68 3F 4B 7E B1 3D 33 33 B3 3D 00 00 C8 41", "D9 44 24 0C D8 0D ?? ?? ?? ?? 8B 44 24 2C 50 51", "D9 44 24 1C D8 0D ?? ?? ?? ?? D8 44 24 28");
		if (Memory::AreAllSignaturesValid(HUDElementsScansResult) == true)
		{
			spdlog::info("(HUD) Loading Bar X Address: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD1Scan] + 12 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Red Cross and Menu Text Width Address: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD1Scan] + 20 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Left Margin: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD2Scan] + 7 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass Needle X: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD3Scan] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Objective Direction Width: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD3Scan] + 36 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass X: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD3Scan] + 48 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass Width: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD3Scan] + 56 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Weapons List Width: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD3Scan] + 72 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Text Width: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD4Scan] + 3 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Ammo X: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD4Scan] + 47 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Ammo Width: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD4Scan] + 55 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Compass Needle Width: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD5Scan] + 2 - (std::uint8_t*)exeModule);

			spdlog::info("(HUD) Unknown Element 1 Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDElementsScansResult[HUD6Scan] + 4 - (std::uint8_t*)exeModule);

			fHUDWidth = 600.0f * fNewAspectRatio;

			// Loading bar X
			fLoadingBarX = 222.0f + (fHUDWidth - 800.0f) / 2.0f;

			Memory::Write(HUDElementsScansResult[HUD1Scan] + 12, fLoadingBarX);

			// Red cross and menu text width
			fRedCrossAndMenuTextWidth = 0.001666f / fNewAspectRatio;

			Memory::Write(HUDElementsScansResult[HUD1Scan] + 20, fRedCrossAndMenuTextWidth);

			// HUD left margin
			fHUDLeftMargin = 0.0333333f / fNewAspectRatio + fHUDMargin / fHUDWidth;

			Memory::Write(HUDElementsScansResult[HUD2Scan] + 7, fHUDLeftMargin);

			// Compass needle X
			fCompassNeedleX = (fHUDWidth - 79.0f - fHUDMargin) / fHUDWidth;

			Memory::Write(HUDElementsScansResult[HUD3Scan] + 4, fCompassNeedleX);

			// Objective direction width
			dObjectiveDirectionWidth = 0.001666 / static_cast<double>(fNewAspectRatio);

			Memory::Write(HUDElementsScansResult[HUD3Scan] + 36, dObjectiveDirectionWidth);

			// Compass X
			fCompassX = (fHUDWidth - 118.0f - fHUDMargin) / fHUDWidth;

			Memory::Write(HUDElementsScansResult[HUD3Scan] + 48, fCompassX);

			// Compass width
			fCompassWidth = 0.163333f / fNewAspectRatio;

			Memory::Write(HUDElementsScansResult[HUD3Scan] + 56, fCompassWidth);

			// Weapons list width
			fWeaponsListWidth = 0.17f / fNewAspectRatio;

			Memory::Write(HUDElementsScansResult[HUD3Scan] + 72, fWeaponsListWidth);

			// HUD text width
			dHUDTextWidth = 0.0006666666666 / static_cast<double>(fNewAspectRatio);

			Memory::Write(HUDElementsScansResult[HUD4Scan] + 3, dHUDTextWidth);

			// Ammo X
			fAmmoX = (fHUDWidth - 74.0f - fHUDMargin) / fHUDWidth;

			Memory::Write(HUDElementsScansResult[HUD4Scan] + 47, fAmmoX);

			// Ammo width
			fAmmoWidth = 0.116666f / fNewAspectRatio;

			Memory::Write(HUDElementsScansResult[HUD4Scan] + 55, fAmmoWidth);			

			// Compass needle width
			Memory::PatchBytes(HUDElementsScansResult[HUD5Scan] + 4, "\x90\x90\x90\x90\x90\x90", 6);

			fCompassNeedleWidth = 0.0333333f / fNewAspectRatio;

			CompassNeedleWidthInstructionHook = safetyhook::create_mid(HUDElementsScansResult[HUD5Scan] + 4, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fCompassNeedleWidth);
			});

			// Unknown HUD element #1
			Memory::PatchBytes(HUDElementsScansResult[HUD6Scan] + 4, "\x90\x90\x90\x90\x90\x90", 6);

			fUnknownHUDElement1 = 0.025000000372529f;

			UnknownHUDElement1InstructionHook = safetyhook::create_mid(HUDElementsScansResult[HUD6Scan] + 4, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fUnknownHUDElement1);
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