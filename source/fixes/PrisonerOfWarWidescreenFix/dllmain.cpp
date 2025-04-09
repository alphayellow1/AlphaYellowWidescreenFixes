// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "PrisonerOfWarWidescreenFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewAspectRatio;
float fFOVFactor;

// Game detection
enum class Game
{
	POWGAME,
	POWCONFIG,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::POWGAME, {"Prisoner of War", "Colditz.EXE"}},
	{Game::POWCONFIG, {"Prisoner of War - Launcher", "PrisonerLaunchEnglish.EXE"}},
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

void WriteIntAsChar8Digits(std::uint8_t* baseAddress, int value)
{
	std::string strValue = std::to_string(value);

	for (char ch : strValue)
	{
		char8_t char8Value = static_cast<char8_t>(ch);

		Memory::Write(baseAddress, char8Value);

		baseAddress += sizeof(char8_t);
	}
}

void WriteIntAsChar16Digits(std::uint8_t* baseAddress, int value)
{
	std::string strValue = std::to_string(value);

	for (char ch : strValue)
	{
		char16_t char16Value = static_cast<char16_t>(ch);

		Memory::Write(baseAddress, char16Value);

		baseAddress += sizeof(char16_t);
	}
}

int digitCount(int number)
{
	int count = 0;

	do
	{
		number /= 10;
		++count;
	} while (number != 0);

	return count;
}

void FOVFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::POWCONFIG)
		{
			std::uint8_t* Resolution1024x768ScanResult = Memory::PatternScan(exeModule, "31 00 30 00 32 00 34 00 20 00 78 00 20 00 37 00 36 00 38");
			if (Resolution1024x768ScanResult)
			{
				spdlog::info("Resolution 1024x768 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution1024x768ScanResult - (std::uint8_t*)exeModule);

				WriteIntAsChar16Digits(Resolution1024x768ScanResult, iCurrentResX);

				WriteIntAsChar16Digits(Resolution1024x768ScanResult + 14, iCurrentResY);
			}
			else
			{
				spdlog::error("Failed to locate resolution memory address.");
				return;
			}

			std::uint8_t* Resolution1024x768Scan2Result = Memory::PatternScan(exeModule, "2D 72 65 73 3A 31 30 32 34 2C 37 36 38");
			if (Resolution1024x768Scan2Result)
			{
				spdlog::info("Resolution 1024x768 Scan 2: Address is {:s}+{:x}", sExeName.c_str(), Resolution1024x768Scan2Result - (std::uint8_t*)exeModule);

				if (digitCount(iCurrentResX) == 4 && digitCount(iCurrentResY) == 4 || digitCount(iCurrentResY) == 3)
				{
					WriteIntAsChar8Digits(Resolution1024x768Scan2Result + 5, iCurrentResX);

					WriteIntAsChar8Digits(Resolution1024x768Scan2Result + 10, iCurrentResY);
				}

				if (digitCount(iCurrentResX) == 3 && digitCount(iCurrentResY) == 3)
				{
					WriteIntAsChar8Digits(Resolution1024x768Scan2Result + 5, iCurrentResX);

					Memory::PatchBytes(Resolution1024x768Scan2Result + 8, "\x2C", 1);

					WriteIntAsChar8Digits(Resolution1024x768Scan2Result + 9, iCurrentResY);

					Memory::PatchBytes(Resolution1024x768Scan2Result + 12, "\x00", 1);
				}
				
			}
			else
			{
				spdlog::error("Failed to locate resolution memory address.");
				return;
			}
		}

		/*
		if (eGameType == Game::POWGAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fNewCameraFOV = fOriginalCameraHFOV * (fOldAspectRatio / fNewAspectRatio);

			std::uint8_t* CameraHFOVScanResult = Memory::PatternScan(exeModule, "C7 44 24 34 00 00 40 3F 8B 0D 84 3E");
			if (CameraHFOVScanResult)
			{
				spdlog::info("Camera HFOV: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVScanResult + 4 - (std::uint8_t*)exeModule);;

				Memory::Write(CameraHFOVScanResult + 4, fNewCameraFOV);
			}
			else
			{
				spdlog::error("Failed to locate camera HFOV memory address.");
				return;
			}
		}
		*/
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