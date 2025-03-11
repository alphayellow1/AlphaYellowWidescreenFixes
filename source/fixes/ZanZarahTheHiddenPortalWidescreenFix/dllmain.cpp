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
#include <cmath> // For atanf, tanf
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>
#include <tlhelp32.h>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "ZanZarahTheHiddenPortalWidescreenFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewValue1;
float fNewValue2;
float fNewValue3;
float fNewValue4;
float fNewValue5;
float fNewValue6;
float fNewValue7;
float fNewValue8;
float fNewValue9;
float fNewAspectRatio;

// Game detection
enum class Game
{
	ZTHP,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::ZTHP, {"ZanZarah: The Hidden Portal", "zanthp.exe"}},
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

void WidescreenFix()
{
	if (eGameType == Game::ZTHP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewValue1 = 0.75f * (fOldAspectRatio / fNewAspectRatio);

		fNewValue2 = 0.65f * (fOldAspectRatio / fNewAspectRatio);

		fNewValue3 = 0.71f * (fOldAspectRatio / fNewAspectRatio);

		fNewValue4 = 0.72f * (fOldAspectRatio / fNewAspectRatio);

		fNewValue5 = 0.68f * (fOldAspectRatio / fNewAspectRatio);

		fNewValue6 = 0.725f * (fOldAspectRatio / fNewAspectRatio);

		fNewValue7 = 0.685f * (fOldAspectRatio / fNewAspectRatio);

		fNewValue8 = 0.69f * (fOldAspectRatio / fNewAspectRatio);

		std::uint8_t* Pattern1ScanResult = Memory::PatternScan(exeModule, "3B 05 ?? ?? ?? ?? 74 4E 83 4D 10 FF");
		if (Pattern1ScanResult)
		{
			spdlog::info("Pattern 1 detected.");

			Memory::PatchBytes(Pattern1ScanResult + 6, "\x90\x90", 2);
		}
		else
		{
			spdlog::error("Failed to locate pattern 1 memory address.");
		}

		std::uint8_t* Pattern2ScanResult = Memory::PatternScan(exeModule, "8D 75 C4 F3 A5 8D 73 2C 8B CE E8 67 5C 00 00");
		if (Pattern2ScanResult)
		{
			spdlog::info("Pattern 2 detected.");

			Memory::PatchBytes(Pattern2ScanResult + 5, "\xE9\x0E\x2C\x19\x00", 5);
		}
		else
		{
			spdlog::error("Failed to locate pattern 2 memory address.");
		}

		std::uint8_t* Pattern3ScanResult = Memory::PatternScan(exeModule, "8B B0 A0 5C 5A 00 89 71 08 8B B0 A4 5C 5A 00 89 71 0C 8B 80 A8 5C 5A 00 89 41 10 89 51 14 5E");
		if (Pattern3ScanResult)
		{
			spdlog::info("Pattern 3 detected.");

			Memory::PatchBytes(Pattern3ScanResult, "\xE9\x87\xAA\x18\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 31);
		}
		else
		{
			spdlog::error("Failed to locate pattern 3 memory address.");
		}

		std::uint8_t* Pattern4ScanResult = Memory::PatternScan(exeModule, "33 C9 8B 30 89 14 24 89 74 24 04 8B 70 04 89 74 24 08 8B 40 08 89 4C 24 14 89 4C 24 10");
		if (Pattern4ScanResult)
		{
			spdlog::info("Pattern 4 detected.");

			Memory::PatchBytes(Pattern4ScanResult, "\x59\x59\x59\x68\x38\x04\x00\x00\x68\x80\x07\x00\x00\x52\x33\xC9\x89\x4C\x24\x14\x89\x4C\x24\x10\xB8\x20\x00\x00\x00", 29);

			Memory::Write(Pattern4ScanResult + 4, iCurrentResY);

			Memory::Write(Pattern4ScanResult + 9, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate pattern 4 memory address.");
		}

		std::uint8_t* Pattern5ScanResult = Memory::PatternScan(exeModule, "F9 FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");
		if (Pattern5ScanResult)
		{
			spdlog::info("Pattern 5 detected.");

			Memory::PatchBytes(Pattern5ScanResult, "\xF9\xFF\x00\x8D\x73\x2C\x8B\xCE\xC7\x44\x24\x08\x80\x07\x00\x00\xC7\x44\x24\x0C\x38\x04\x00\x00\xE9\xD8\xD3\xE6\xFF\x00\x00\x00\xC7\x41\x08\x80\x07\x00\x00\xC7\x41\x0C\x38\x04\x00\x00\xC7\x41\x10\x20\x00\x00\x00\xC7\x41\x14\x00\x00\x00\x00\x5E\xC2\x04", 63);
		}
		else
		{
			spdlog::error("Failed to locate pattern 5 memory address.");
		}

		std::uint8_t* Pattern6ScanResult = Memory::PatternScan(exeModule, "DA E1 42 00 00 00 40 3F");
		if (Pattern6ScanResult)
		{
			spdlog::info("Pattern 6 detected.");

			Memory::Write(Pattern6ScanResult + 4, fNewValue1);
		}
		else
		{
			spdlog::error("Failed to locate pattern 6 memory address.");
		}

		std::uint8_t* Pattern7ScanResult = Memory::PatternScan(exeModule, "7D 3F 66 66 26 3F 00 00");
		if (Pattern7ScanResult)
		{
			spdlog::info("Pattern 7 detected.");

			Memory::Write(Pattern7ScanResult + 2, fNewValue2);
		}
		else
		{
			spdlog::error("Failed to locate pattern 7 memory address.");
		}

		std::uint8_t* Pattern8ScanResult = Memory::PatternScan(exeModule, "66 66 A6 3F 00 00 00 00 00 00 40 3F 00 00 80 3F");
		if (Pattern8ScanResult)
		{
			spdlog::info("Pattern 8 detected.");

			Memory::Write(Pattern8ScanResult + 8, fNewValue1);
		}
		else
		{
			spdlog::error("Failed to locate pattern 8 memory address.");
		}

		std::uint8_t* Pattern9ScanResult = Memory::PatternScan(exeModule, "00 00 C0 BF AB AA AA 3E 66 66 E6 3F 8F C2 35 3F");
		if (Pattern9ScanResult)
		{
			spdlog::info("Pattern 9 detected.");

			Memory::Write(Pattern9ScanResult + 12, fNewValue3);
		}
		else
		{
			spdlog::error("Failed to locate pattern 9 memory address.");
		}

		std::uint8_t* Pattern10ScanResult = Memory::PatternScan(exeModule, "EC 51 38 3F 7B 14 2E 3F C3 F5 08 3F A6 9B C4 3B 0A D7 23 3B CD CC 0C 3F 9A 99 39 3F 29 5C 2F 3F AD AC EF B7 17 B7 D1 37 7B 14 0E 3F 00 00 C0 3E E1 7A 14 3E D7 A3 30 3F");
		if (Pattern10ScanResult)
		{
			spdlog::info("Pattern 10 detected.");

			Memory::Write(Pattern10ScanResult, fNewValue4);

			Memory::Write(Pattern10ScanResult + 4, fNewValue5);

			Memory::Write(Pattern10ScanResult + 18, fNewValue6);

			Memory::Write(Pattern10ScanResult + 28, fNewValue7);

			Memory::Write(Pattern10ScanResult + 52, fNewValue8);
		}
		else
		{
			spdlog::error("Failed to locate pattern 10 memory address.");
		}

		std::uint8_t* Pattern11ScanResult = Memory::PatternScan(exeModule, "F9 FF 00 8D 73 2C 8B CE C7 44 24 08 80 07 00 00 C7 44 24 0C 38 04 00 00 E9 D8 D3 E6 FF 00 00 00 C7 41 08 80 07 00 00 C7 41 0C 38 04 00 00 C7 41 10 20 00 00 00 C7 41 14 00 00 00 00 5E C2 04");
		if (Pattern11ScanResult)
		{
			spdlog::info("Pattern 11 detected.");

			Memory::Write(Pattern11ScanResult + 12, iCurrentResX);

			Memory::Write(Pattern11ScanResult + 20, iCurrentResY);

			Memory::Write(Pattern11ScanResult + 35, iCurrentResX);

			Memory::Write(Pattern11ScanResult + 42, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate pattern 11 memory address.");
		}

		std::uint8_t* Pattern12ScanResult = Memory::PatternScan(exeModule, "E1 42 00 DA E1 42 00 00 00 40 3F 00 00 00 00 00");
		if (Pattern12ScanResult)
		{
			spdlog::info("Pattern 12 detected.");

			fNewValue9 = 0.625f; // 750.0f / 1200.0f;

			Memory::Write(Pattern12ScanResult + 7, fNewValue9);
		}
		else
		{
			spdlog::error("Failed to locate pattern 12 memory address.");
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