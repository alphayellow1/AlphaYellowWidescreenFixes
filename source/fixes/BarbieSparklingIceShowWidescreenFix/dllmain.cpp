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
#include <cmath> // For atanf, tanf
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <vector>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "BarbieSparklingIceShowWidescreenFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
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
	BSIS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BSIS, {"Barbie Sparkling Ice Show", "IceSkating.exe"}},
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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds : [fNewAspectRatio]
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fNewCameraFOV = (1.0f / fFOVFactor) * (fOldAspectRatio / fNewAspectRatio);

	_asm
	{
		fdivr dword ptr ds: [fNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::BSIS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);
		
		std::uint8_t* ResolutionWidthInstructionScanResult = Memory::PatternScan(exeModule, "8B 3D 68 10 49 00 8B 74 24 10 8B 0D 00 E2 49 00");
		if (ResolutionWidthInstructionScanResult)
		{
			spdlog::info("Resolution Width Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstructionScanResult + 8 - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstructionMidHook{};

			ResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionWidthInstructionScanResult, [](SafetyHookContext& ctx)
				{
					*reinterpret_cast<uint32_t*>(0x00491068) = iCurrentResX;
				});
		}
		else
		{
			spdlog::error("Failed to locate resolution width instruction memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstructionScanResult = Memory::PatternScan(exeModule, "8B 35 6C 10 49 00 8B D6 2B 54 24 1C");
		if (ResolutionHeightInstructionScanResult)
		{
			spdlog::info("Resolution Height Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstructionMidHook{};

			ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionHeightInstructionScanResult, [](SafetyHookContext& ctx)
				{
					*reinterpret_cast<uint32_t*>(0x0049106C) = iCurrentResY;
				});
		}
		else
		{
			spdlog::error("Failed to locate resolution height instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D 18 59 48 00 D8 8E 6C 02 00 00 D9 9E 1C 01 00 00 D9 96 30 01 00 00");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult + 6, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 3D 2C 52 48 00 D9 C0 D8 8E 68 02 00 00 D9 1B");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 6, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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