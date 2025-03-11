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
std::string sFixName = "SecretAgentBarbieWidescreenFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
uint8_t newWidthSmall;
uint8_t newWidthBig;
uint8_t newHeightSmall;
uint8_t newHeightBig;
float fNewAspectRatio;
float fFOVFactor;
float fModifiedFOVValue;

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
	SAB,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SAB, {"Secret Agent Barbie", "SecretAgent.exe"}},
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

float CalculateNewFOV(float fCurrentFOV)
{
	return fFOVFactor * (2.0f * atanf(tanf(fCurrentFOV / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
}

void WidescreenFix()
{
	if (eGameType == Game::SAB && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 54 24 08 89 81 B0 00 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			static float fLastModifiedFOV = 0.0f;

			static std::vector<float> computedFOVs;

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV = std::bit_cast<float>(ctx.eax);

				// Checks if this FOV has already been computed
				if (std::find(computedFOVs.begin(), computedFOVs.end(), fCurrentCameraFOV) != computedFOVs.end())
				{
					// Value already processed, then skips the calculations
					return;
				}

				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fModifiedFOVValue = CalculateNewFOV(fCurrentCameraFOV);

				// If the new computed value is different, updates the FOV value
				if (fCurrentCameraFOV != fModifiedFOVValue)
				{
					fCurrentCameraFOV = fModifiedFOVValue;
					fLastModifiedFOV = fModifiedFOVValue;
				}

				// Stores the new value so future calls can skip re-calculations
				computedFOVs.push_back(fModifiedFOVValue);

				ctx.eax = std::bit_cast<uintptr_t>(fCurrentCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* ResolutionsInstructionScanResult = Memory::PatternScan(exeModule, "8B 4C 24 04 8B 54 24 08 89 48 40 8B 4C 24 0C 89 50 44 8B 54 24 10 89 48 48 89 50 4C");
		if (ResolutionsInstructionScanResult)
		{
			spdlog::info("Resolution Width Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionsInstructionScanResult + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Height Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionsInstructionScanResult + 15 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionsInstructionScanResult + 8, "\x90\x90\x90", 3);

			Memory::PatchBytes(ResolutionsInstructionScanResult + 15, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstructionMidHook{};

			ResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionsInstructionScanResult + 11, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x40) = iCurrentResX;
			});

			static SafetyHookMid ResolutionHeightInstructionMidHook{};

			ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionsInstructionScanResult + 18, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x44) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolutions instruction memory address.");
			return;
		}

		/*
		newWidthSmall = iCurrentResX % 256;

		newWidthBig = (iCurrentResX - newWidthSmall) / 256;

		newHeightSmall = iCurrentResY % 256;

		newHeightBig = (iCurrentResY - newHeightSmall) / 256;

		Memory::Write(NewCodecaveScanResult + 3, newWidthSmall);

		Memory::Write(NewCodecaveScanResult + 4, newWidthBig);

		Memory::Write(NewCodecaveScanResult + 14, newHeightSmall);

		Memory::Write(NewCodecaveScanResult + 15, newHeightBig);
		*/
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