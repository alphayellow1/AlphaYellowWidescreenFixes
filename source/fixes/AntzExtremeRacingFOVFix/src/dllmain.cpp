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
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "AntzExtremeRacingFOVFix";
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
constexpr float fOriginalMenuCameraFOV = 0.5f;
constexpr float fOriginalCameraHFOV = 0.5f;
constexpr float fOriginalCameraVFOV = 0.375f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewMenuFOV;
float fValue1;
float fValue2;
float fValue3;
float fValue4;

// Game detection
enum class Game
{
	AER,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AER, {"Antz Extreme Racing", "antzextremeracing.exe"}},
};

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
	GetModuleFileNameW(dllModule, exePathW, MAX_PATH);
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
		spdlog::info("Module Address: 0x{0:X}", (uintptr_t)dllModule);
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

void FOVFix()
{
	if (eGameType == Game::AER && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 40 3C D8 49 28");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.eax + 0x3C) = fOriginalCameraHFOV * (fNewAspectRatio / fOldAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 42 40 D8 48 28");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.edx + 0x40) = fOriginalCameraVFOV * (fNewAspectRatio / fOldAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction memory address.");
			return;
		}

		fNewMenuFOV = fOriginalMenuCameraFOV * (fNewAspectRatio / fOldAspectRatio);

		std::uint8_t* MenuAspectRatioAndFOVScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 10 68 6C 34 58 00 E8 00 1F 00 00 83 C4 04");
		if (MenuAspectRatioAndFOVScanResult)
		{
			spdlog::info("Menu Aspect Ratio & FOV: Address is {:s}+{:x}", sExeName.c_str(), MenuAspectRatioAndFOVScanResult - (std::uint8_t*)exeModule);

			Memory::Write(MenuAspectRatioAndFOVScanResult + 0x1, fNewAspectRatio);

			Memory::Write(MenuAspectRatioAndFOVScanResult + 0x6, fNewMenuFOV);
		}
		else
		{
			spdlog::error("Failed to locate menu aspect ratio & FOV scan 1 memory address.");
			return;
		}

		std::uint8_t* MenuAspectRatioAndFOV2ScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A 00 68 AC CD BC 00 E8 2C 8F F8 FF 83 C4 10 8B 0D A0 7A 57 00");
		if (MenuAspectRatioAndFOV2ScanResult)
		{
			spdlog::info("Menu Aspect Ratio & FOV 2: Address is {:s}+{:x}", sExeName.c_str(), MenuAspectRatioAndFOV2ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(MenuAspectRatioAndFOV2ScanResult + 0x1, fNewAspectRatio);

			Memory::Write(MenuAspectRatioAndFOV2ScanResult + 0x6, fNewMenuFOV);
		}
		else
		{
			spdlog::error("Failed to locate menu aspect ratio & FOV scan 2 memory address.");
			return;
		}

		std::uint8_t* Value1ScanResult = Memory::PatternScan(exeModule, "10 8B 45 08 C7 40 28 CD CC CC 3F 6A 00 8B");
		if (Value1ScanResult)
		{
			spdlog::info("Value 1: Address is {:s}+{:x}", sExeName.c_str(), Value1ScanResult - (std::uint8_t*)exeModule);

			fValue1 = 1.6f * (fNewAspectRatio / fOldAspectRatio);

			Memory::Write(Value1ScanResult + 0x7, fValue1);
		}
		else
		{
			spdlog::error("Failed to locate value 1 memory address.");
			return;
		}

		std::uint8_t* Value2ScanResult = Memory::PatternScan(exeModule, "BF 00 C7 45 F4 00 00 80 3F E9 A0 01");
		if (Value2ScanResult)
		{
			spdlog::info("Value 2: Address is {:s}+{:x}", sExeName.c_str(), Value2ScanResult - (std::uint8_t*)exeModule);

			fValue2 = 1.0f * (fNewAspectRatio / fOldAspectRatio);

			Memory::Write(Value2ScanResult + 0x5, fValue2);
		}
		else
		{
			spdlog::error("Failed to locate value 2 memory address.");
			return;
		}

		std::uint8_t* Value3ScanResult = Memory::PatternScan(exeModule, "BF 00 C7 45 F4 00 00 80 3F C7 05 30");
		if (Value3ScanResult)
		{
			spdlog::info("Value 3: Address is {:s}+{:x}", sExeName.c_str(), Value3ScanResult - (std::uint8_t*)exeModule);

			fValue3 = 1.0f * (fNewAspectRatio / fOldAspectRatio);

			Memory::Write(Value3ScanResult + 0x5, fValue3);
		}
		else
		{
			spdlog::error("Failed to locate value 3 memory address.");
			return;
		}

		std::uint8_t* Value4ScanResult = Memory::PatternScan(exeModule, "00 00 00 70 42 00 00 00 00 F4 1D DB 43 F8 B3");
		if (Value4ScanResult)
		{
			spdlog::info("Value 4: Address is {:s}+{:x}", sExeName.c_str(), Value4ScanResult - (std::uint8_t*)exeModule);

			fValue4 = 2.0f * RadToDeg(atanf(tanf(DegToRad(60.0f / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));

			Memory::Write(Value4ScanResult + 0x1, fValue4);
		}
		else
		{
			spdlog::error("Failed to locate value 4 memory address.");
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
