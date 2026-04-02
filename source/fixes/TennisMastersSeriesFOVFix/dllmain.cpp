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
std::string sFixName = "TennisMastersSeriesFOVFix";
std::string sFixVersion = "1.2";
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
constexpr float fOriginalHFOV1 = -0.4f;
constexpr float fOriginalHFOV2 = 0.4f;
constexpr float fOriginalVFOV1 = 0.3f;
constexpr float fOriginalVFOV2 = -0.3f;
constexpr float fOriginalPlayerSelectionHFOV1 = -0.55f;
constexpr float fOriginalPlayerSelectionHFOV2 = 0.55f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV1;
float fNewCameraHFOV2;
float fNewCameraVFOV1;
float fNewCameraVFOV2;
float fNewPlayerSelectionHFOV1;
float fNewPlayerSelectionHFOV2;

// Game detection
enum class Game
{
	TMS,
	Unknown
};

enum CameraHFOVInstructionsIndex
{
	Camera1,
	Camera2,
	Camera3,
	Camera4,
	Camera5,
	Camera6,
	Camera7,
	PlayerSelection
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TMS, {"Tennis Masters Series", "Tennis Masters Series.exe"}},
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
	if (eGameType == Game::TMS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 8D 44 8B", "D8 0D ?? ?? ?? ?? 8D 44 86",
		"D8 0D ?? ?? ?? ?? 8D 44 81", "43 ?? E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ??", "D8 0D ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? D9 5C 24",
		"86 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ??",
		"CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ??",
		"C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 8B 8E");
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instructions Scan 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera5] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera6] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions Scan 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera7] - (std::uint8_t*)exeModule);

			spdlog::info("Player Selection FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[PlayerSelection] - (std::uint8_t*)exeModule);

			fNewCameraHFOV1 = fOriginalHFOV1 * fAspectRatioScale * fFOVFactor;

			fNewCameraHFOV2 = fOriginalHFOV2 * fAspectRatioScale * fFOVFactor;

			fNewCameraVFOV1 = fOriginalVFOV1 * fFOVFactor;

			fNewCameraVFOV2 = fOriginalVFOV2 * fFOVFactor;

			fNewPlayerSelectionHFOV1 = fOriginalPlayerSelectionHFOV1 * fAspectRatioScale;

			fNewPlayerSelectionHFOV2 = fOriginalPlayerSelectionHFOV2 * fAspectRatioScale;

			// Camera 1
			Memory::Write(CameraFOVInstructionsScansResult[Camera1] + 2, &fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera1] + 21, &fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[Camera1] + 33, &fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera1] + 45, &fNewCameraVFOV2);

			// Camera 2
			Memory::Write(CameraFOVInstructionsScansResult[Camera2] + 2, &fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera2] + 18, &fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[Camera2] + 30, &fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera2] + 42, &fNewCameraVFOV2);

			// Camera 3
			Memory::Write(CameraFOVInstructionsScansResult[Camera3] + 2, &fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera3] + 18, &fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[Camera3] + 30, &fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera3] + 42, &fNewCameraVFOV2);

			// Camera 4
			Memory::Write(CameraFOVInstructionsScansResult[Camera4] + 15, &fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera4] + 47, &fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[Camera4] + 63, &fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera4] + 79, &fNewCameraVFOV2);

			// Camera 5
			Memory::Write(CameraFOVInstructionsScansResult[Camera5] + 2, &fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera5] + 20, &fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[Camera5] + 32, &fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera5] + 42, &fNewCameraVFOV2);

			// Camera 6
			Memory::Write(CameraFOVInstructionsScansResult[Camera6] + 32, &fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera6] + 64, &fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[Camera6] + 80, &fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera6] + 96, &fNewCameraVFOV2);

			// Camera 7
			Memory::Write(CameraFOVInstructionsScansResult[Camera7] + 28, &fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera7] + 60, &fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[Camera7] + 76, &fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[Camera7] + 92, &fNewCameraVFOV2);

			// Player Selection Camera
			Memory::Write(CameraFOVInstructionsScansResult[PlayerSelection] + 6, fNewPlayerSelectionHFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[PlayerSelection] + 16, fNewPlayerSelectionHFOV2);
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
