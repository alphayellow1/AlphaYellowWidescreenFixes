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
HMODULE dllModule2;

// Fix details
std::string sFixName = "RolandGarrosFrenchOpen2001WidescreenFix";
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
int iNewMenuResX;
int iNewMenuResY;
int iNewMatchResX;
int iNewMatchResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;

// Game detection
enum class Game
{
	RGFO2001,
	Unknown
};

enum MenuResolutionInstructionsScan
{
	MenuResolution1Scan,
	MenuResolution2Scan,
	MenuResolution3Scan,
	MenuResolution4Scan,
	MenuResolution5Scan,
	MenuResolution6Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RGFO2001, {"Roland Garros French Open 2001", "RG2001.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "MenuResolutionWidth", iNewMenuResX);
	inipp::get_value(ini.sections["Settings"], "MenuResolutionHeight", iNewMenuResY);
	inipp::get_value(ini.sections["Settings"], "MatchResolutionWidth", iNewMatchResX);
	inipp::get_value(ini.sections["Settings"], "MatchResolutionHeight", iNewMatchResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iNewMenuResX);
	spdlog_confparse(iNewMenuResY);
	spdlog_confparse(iNewMatchResX);
	spdlog_confparse(iNewMatchResY);
	spdlog_confparse(fFOVFactor);

	// If resolution not specified, use desktop resolution
	if ((iNewMatchResX <= 0 || iNewMatchResY <= 0) || (iNewMenuResX <= 0 || iNewMenuResY <= 0))
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();

		if (iNewMenuResX <= 0 || iNewMenuResY <= 0)
		{
			iNewMenuResX = desktopDimensions.first;
			iNewMenuResY = desktopDimensions.second;
			spdlog_confparse(iNewMenuResX);
			spdlog_confparse(iNewMenuResY);
		}
		else if (iNewMatchResX <= 0 || iNewMatchResY <= 0)
		{
			iNewMatchResX = desktopDimensions.first;
			iNewMatchResY = desktopDimensions.second;
			spdlog_confparse(iNewMatchResX);
			spdlog_confparse(iNewMatchResY);
		}
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

	while ((dllModule2 = GetModuleHandleA("rcMain.dll")) == nullptr)
	{
		spdlog::warn("rcMain.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for rcMain.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraHFOVInstructionHook{};

void CameraHFOVInstructionMidHook(safetyhook::Context& ctx)
{
	float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.esi + 0x144);

	fNewCameraHFOV = (fCurrentCameraHFOV / fAspectRatioScale) / fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV]
	}
}

static SafetyHookMid CameraVFOVInstructionHook{};

void CameraVFOVInstructionMidHook(safetyhook::Context& ctx)
{
	float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.esi + 0x148);

	fNewCameraVFOV = fCurrentCameraVFOV / fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::RGFO2001 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iNewMatchResX) / static_cast<float>(iNewMatchResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> MenuResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF D5 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF D7 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 15", "81 38 ?? ?? ?? ?? 0F 95 ?? 84 C0 74 ?? 8B 0D",
		                                                                                       dllModule2, "8B 10 89 11 8B 40 ?? 89 41 ?? C2 ?? ?? 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 8B 44 24 ?? 89 41");
		if (Memory::AreAllSignaturesValid(MenuResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuResolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuResolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuResolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuResolution4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuResolution5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 6 Scan: Address is rcMain.dll+{:x}", MenuResolutionInstructionsScansResult[MenuResolution6Scan] - (std::uint8_t*)dllModule2);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution1Scan] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution1Scan] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution2Scan] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution2Scan] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution3Scan] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution3Scan] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution4Scan] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution4Scan] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuResolution5Scan] + 2, iNewMenuResX);
		}

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "66 C7 41 58 90 01 66 C7 41 5A 2C 01 C2 04 00 66 C7 41 58 00 02 66 C7 41 5A 80 01 C2 04 00 66 C7 41 58 80 02 66 C7 41 5A 90 01 C2 04 00 66 C7 41 58 80 02 66 C7 41 5A E0 01 C2 04 00 66 C7 41 58 20 03 66 C7 41 5A 58 02 C2 04 00 66 C7 41 58 00 04 66 C7 41 5A 00 03 C2 04 00 66 C7 41 58 00 05 66 C7 41 5A 00 04 C2 04 00 66 C7 41 58 40 06 66 C7 41 5A B0 04");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionListScanResult + 4, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 10, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 19, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 25, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 34, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 40, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 49, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 55, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 64, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 70, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 79, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 85, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 94, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 100, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 109, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 115, (uint16_t)iNewMatchResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list memory address.");
			return;
		}		

		// Located in rcMain.Runn::RCamera::think
		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 86 44 01 00 00 D8 A6 3C 01 00 00 D8 C9 D8 86 3C 01 00 00 D9 54 24 04 D9 9E 3C 01 00 00 D9 86 48 01 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction Scan: Address is rcMain.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			spdlog::info("Camera VFOV Instruction Scan: Address is rcMain.dll+{:x}", CameraFOVInstructionScanResult + 30 - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraHFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionScanResult + 30, "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 30, CameraVFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instructions scan memory address.");
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