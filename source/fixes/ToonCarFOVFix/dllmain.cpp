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
#include <cmath>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "ToonCarFOVFix";
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
constexpr float fOriginalAspectRatio = 1.0f;
constexpr float fOriginalMenuCameraFOV = 1.5707963268f;
constexpr float fOriginalGameplayCameraFOV = 1.745329252f;
constexpr float fOriginalAnnouncerFOV = 0.7853981852531433f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fNewAspectRatio2;
float fAspectRatioScale;
float fNewMenuCameraFOV;
float fNewGameplayCameraFOV;
float fNewAnnouncerFOV;

// Game detection
enum class Game
{
	TC,
	Unknown
};

enum AspectRatioInstructionsScans
{
	MenuAspectRatioScan,
	GameplayAspectRatioScan,
	AnnouncerAspectRatioScan
};

enum CameraFOVInstructionsScans
{
	MenuCameraFOVScan,
	GameplayCameraFOVScan,
	AnnouncerCameraFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TC, {"Toon Car", "ToonCar.exe"}},
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

void FOVFix()
{
	if (eGameType == Game::TC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 D8 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53 53 8B C8 E8 ?? ?? ?? ?? EB ?? 33 C0 DB 05 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 83 CD", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Menu Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[MenuAspectRatioScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAspectRatioScan] - (std::uint8_t*)exeModule);

			spdlog::info("Announcer Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AnnouncerAspectRatioScan] - (std::uint8_t*)exeModule);
			
			fNewAspectRatio2 = fOriginalAspectRatio * fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[MenuAspectRatioScan] + 1, fNewAspectRatio2);

			Memory::Write(AspectRatioInstructionsScansResult[GameplayAspectRatioScan] + 1, fNewAspectRatio2);

			Memory::Write(AspectRatioInstructionsScansResult[AnnouncerAspectRatioScan] + 1, fNewAspectRatio2);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 51 D8 0D", "68 ?? ?? ?? ?? 51 83 CD", "68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? D8 0D");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Menu Camera FOV Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MenuCameraFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayCameraFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Announcer Camera FOV Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[AnnouncerCameraFOVScan] - (std::uint8_t*)exeModule);

			fNewMenuCameraFOV = Maths::CalculateNewFOV_RadBased(fOriginalMenuCameraFOV, fAspectRatioScale);

			fNewGameplayCameraFOV = Maths::CalculateNewFOV_RadBased(fOriginalGameplayCameraFOV, fAspectRatioScale) * fFOVFactor;

			fNewAnnouncerFOV = Maths::CalculateNewFOV_RadBased(fOriginalAnnouncerFOV, fAspectRatioScale);

			Memory::Write(CameraFOVInstructionsScansResult[MenuCameraFOVScan] + 1, fNewMenuCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[GameplayCameraFOVScan] + 1, fNewGameplayCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[AnnouncerCameraFOVScan] + 1, fNewAnnouncerFOV);
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