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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "HugoBukkazoomWidescreenFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewCameraHFOV;
float fNewCameraFOV1;
float fNewCameraFOV2;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
	return degrees * (fPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
	return radians * (180.0f / fPi);
}

// Game detection
enum class Game
{
	CONFIG,
	HB,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CONFIG, {"Hugo: Bukkazoom! - Configuration", "Config.exe"}},
	{Game::HB, {"Hugo: Bukkazoom!", "Runtime.exe"}},
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
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);

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
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));
}

static SafetyHookMid GameplayCameraHFOVInstructionHook{};

void GameplayCameraHFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fNewCameraHFOV = 1.125 * fNewAspectRatio - 0.75f;

	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

void WidescreenFix()
{
	if (bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		if (eGameType == Game::CONFIG)
		{
			std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "89 10 89 70 04 5F 5E C3 90 90 90 90");
			if (ResolutionInstructionsScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

				static SafetyHookMid ResolutionHeightInstructionMidHook{};

				ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 2, [](SafetyHookContext& ctx)
				{
					ctx.esi = std::bit_cast<uint32_t>(iCurrentResY);
				});

				static SafetyHookMid ResolutionWidthInstructionMidHook{};

				ResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
				{
					ctx.edx = std::bit_cast<uint32_t>(iCurrentResX);
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}
		}

		if (eGameType == Game::HB)
		{
			std::uint8_t* GameplayCameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 05 ?? ?? ?? ?? D9 54 24 28 D9 E0 D9 5C 24 2C");
			if (GameplayCameraHFOVInstructionScanResult)
			{
				spdlog::info("Gameplay Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(GameplayCameraHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				GameplayCameraHFOVInstructionHook = safetyhook::create_mid(GameplayCameraHFOVInstructionScanResult + 6, GameplayCameraHFOVInstructionMidHook);
			}
			else
			{
				spdlog::error("Failed to locate gameplay camera HFOV instruction memory address.");
				return;
			}

			std::uint8_t* MainMenuAspectRatioandFOV1ScanResult = Memory::PatternScan(exeModule, "68 CD CC CC 3D 68 AB AA AA 3F 68 00 00 80 BF 68 00 00 34 42 8B CD");
			if (MainMenuAspectRatioandFOV1ScanResult)
			{
				spdlog::info("Main Menu Aspect Ratio and FOV 1: Address is {:s}+{:x}", sExeName.c_str(), MainMenuAspectRatioandFOV1ScanResult - (std::uint8_t*)exeModule);

				Memory::Write(MainMenuAspectRatioandFOV1ScanResult + 6, fNewAspectRatio);

				fNewCameraFOV1 = CalculateNewFOV(45.0f);

				Memory::Write(MainMenuAspectRatioandFOV1ScanResult + 16, fNewCameraFOV1);
			}
			else
			{
				spdlog::error("Failed to locate main menu aspect ratio and FOV 1 memory address.");
				return;
			}

			std::uint8_t* MainMenuAspectRatioandFOV2ScanResult = Memory::PatternScan(exeModule, "7A 44 68 CD CC CC 3D 68 AB AA AA 3F 68 00 00 80 BF 8D B7 74 01 00 00 68 00 00 96 42 8B CE FF 15");
			if (MainMenuAspectRatioandFOV2ScanResult)
			{
				spdlog::info("Main Menu Aspect Ratio and FOV 2: Address is {:s}+{:x}", sExeName.c_str(), MainMenuAspectRatioandFOV2ScanResult - (std::uint8_t*)exeModule);

				Memory::Write(MainMenuAspectRatioandFOV2ScanResult + 8, fNewAspectRatio);

				fNewCameraFOV2 = CalculateNewFOV(75.0f);

				Memory::Write(MainMenuAspectRatioandFOV2ScanResult + 24, fNewCameraFOV2);
			}
			else
			{
				spdlog::error("Failed to locate main menu aspect ratio and FOV 2 memory address.");
				return;
			}
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