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
std::string sFixName = "RCDaredevilFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalCameraFOV = 75.0f;
constexpr float fTolerance = 0.001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewAspectRatio2;
float fFOVFactor;
float fNewCameraFOV;

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
	RCD,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RCD, {"RC Daredevil", "RC Daredevil.exe"}},
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
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}
}

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));
}

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds : [fNewAspectRatio2]
	}
}

void FOVFix()
{
	if (eGameType == Game::RCD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 4C 24 08 51 DC 0D ?? ?? ?? ?? DC 0D ?? ?? ?? ??");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			if (std::fabs(fNewAspectRatio - (5.0f / 4.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 1.05f;
				spdlog::info("Aspect ratio is 5:4.");
			}
			else if (std::fabs(fNewAspectRatio - (4.0f / 3.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 1.0f;
				spdlog::info("Aspect ratio is 4:3.");
			}
			else if (std::fabs(fNewAspectRatio - (3.0f / 2.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.9190000892f;
				spdlog::info("Aspect ratio is 3:2.");
			}
			else if (std::fabs(fNewAspectRatio - (16.0f / 10.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.8793799281f;
				spdlog::info("Aspect ratio is 16:10.");
			}
			else if (std::fabs(fNewAspectRatio - (15.0f / 9.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.8560000658f;
				spdlog::info("Aspect ratio is 15:9.");
			}
			else if (std::fabs(fNewAspectRatio - (16.0f / 9.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.8212999701f;
				spdlog::info("Aspect ratio is 16:9.");
			}
			else if (std::fabs(fNewAspectRatio - (1.85f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.8026000261f;
				spdlog::info("Aspect ratio is 1.85:1.");
			}
			else if (std::fabs(fNewAspectRatio - (2560.0f / 1080.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.6975000501f;
				spdlog::info("Aspect ratio is 64:27 (2560x1080).");
			}
			else if ((std::fabs(fNewAspectRatio - (3440.0f / 1440.0f)) < fTolerance) ||
				(std::fabs(fNewAspectRatio - 2.39f) < fTolerance))
			{
				fNewAspectRatio2 = 0.6948000789f;
				spdlog::info("Aspect ratio is 21:9 (3440x1440) or 2.39:1.");
			}
			else if (std::fabs(fNewAspectRatio - (3840.0f / 1600.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.6939001083f;
				spdlog::info("Aspect ratio is 21:9 (3840x1600).");
			}
			else if (std::fabs(fNewAspectRatio - (2.76f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.6500000358f;
				spdlog::info("Aspect ratio is 2.76:1.");
			}
			else if (std::fabs(fNewAspectRatio - (32.0f / 10.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.6100006104f;
				spdlog::info("Aspect ratio is 32:10.");
			}
			else if (std::fabs(fNewAspectRatio - (32.0f / 9.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.5863001347f;
				spdlog::info("Aspect ratio is 32:9.");
			}
			else if (std::fabs(fNewAspectRatio - (15.0f / 4.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.5750004053f;
				spdlog::info("Aspect ratio is 15:4.");
			}
			else if (std::fabs(fNewAspectRatio - (12.0f / 3.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.5638000965f;
				spdlog::info("Aspect ratio is 12:3.");
			}
			else if (std::fabs(fNewAspectRatio - (48.0f / 10.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.5348800421f;
				spdlog::info("Aspect ratio is 48:10.");
			}
			else if (std::fabs(fNewAspectRatio - (45.0f / 9.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.5292950273f;
				spdlog::info("Aspect ratio is 45:9.");
			}
			else if (std::fabs(fNewAspectRatio - (48.0f / 9.0f)) < fTolerance)
			{
				fNewAspectRatio2 = 0.5212549567f;
				spdlog::info("Aspect ratio is 48:9.");
			}
			else
			{
				spdlog::warn("Aspect ratio not supported, exiting the fix.");
				return;
			}

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult + 4, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVScanResult = Memory::PatternScan(exeModule, "C7 44 24 1C 00 00 96 42 75 08 8B 06 55 8B CE FF 50 18");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV = CalculateNewFOV(fOriginalCameraFOV) * fFOVFactor;

			Memory::Write(CameraFOVScanResult + 4, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV scan memory address.");
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