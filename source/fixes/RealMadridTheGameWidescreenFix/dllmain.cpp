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
std::string sFixName = "RealMadridTheGameWidescreenFix";
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

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;

// Game detection
enum class Game
{
	RMTG,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RMTG, {"Real Madrid: The Game", "Game.exe"}},
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
	if (eGameType == Game::RMTG && bFixActive == true)
	{
		std::uint8_t* Resolution1ScanResult = Memory::PatternScan(exeModule, "20 A3 59 00 11 00 00 00 20 03 00 00 58 02 00 00 3C 00 00 00");
		if (Resolution1ScanResult)
		{
			spdlog::info("Resolution 1: Address is {:s}+{:x}", sExeName.c_str(), Resolution1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution1ScanResult + 8, iCurrentResX);

			Memory::Write(Resolution1ScanResult + 12, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution 1 memory address.");
			return;
		}

		std::uint8_t* Resolution2ScanResult = Memory::PatternScan(exeModule, "00 01 00 00 00 20 03 00 00 58 02 00 00 00 00 00 00 01 00 00 00 00 04 00 00 00 03 00");
		if (Resolution2ScanResult)
		{
			spdlog::info("Resolution 2: Address is {:s}+{:x}", sExeName.c_str(), Resolution2ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution2ScanResult + 5, iCurrentResX);

			Memory::Write(Resolution2ScanResult + 9, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution 2 memory address.");
			return;
		}

		std::uint8_t* Resolution3ScanResult = Memory::PatternScan(exeModule, "00 00 00 00 00 20 03 00 00 58 02 00 00 00 00 00 00 00 00 00 00 00 04 00 00 00 03 00 00 00 00");
		if (Resolution3ScanResult)
		{
			spdlog::info("Resolution 3: Address is {:s}+{:x}", sExeName.c_str(), Resolution3ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution3ScanResult + 5, iCurrentResX);

			Memory::Write(Resolution3ScanResult + 9, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution 3 memory address.");
			return;
		}

		std::uint8_t* Resolution4ScanResult = Memory::PatternScan(exeModule, "00 00 00 00 00 00 20 03 00 00 58 02 00 00 3C 00 00 00 00 00 00 00 00 04 00 00 00 03 00 00 3C 00");
		if (Resolution4ScanResult)
		{
			spdlog::info("Resolution 4: Address is {:s}+{:x}", sExeName.c_str(), Resolution4ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution4ScanResult + 6, iCurrentResX);

			Memory::Write(Resolution4ScanResult + 10, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution 4 memory address.");
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