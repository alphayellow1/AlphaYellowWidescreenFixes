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
HMODULE dllModule2 = nullptr;
HMODULE pluginModule = nullptr;

// Fix details
std::string sFixName = "DraculaDaysOfGoreWidescreenFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalCameraFOV = 90.0f;

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

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewAspectRatio;
float fFOVFactor;

// Game detection
enum class Game
{
	DATFOD,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DATFOD, {"Dracula: Days of Gore", "Dracula.exe"}},
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
	bool bGameFound = false;

	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			bGameFound = true;
			break;
		}
	}

	if (bGameFound == false)
	{
		spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		return false;
	}

	while ((pluginModule = GetModuleHandleA("DraculaGamePlugin.vplugin")) == nullptr)
	{
		spdlog::warn("DraculaGamePlugin.vplugin not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for DraculaGamePlugin.vplugin: 0x{:X}", reinterpret_cast<uintptr_t>(pluginModule));

	while ((dllModule2 = GetModuleHandleA("vision71.dll")) == nullptr)
	{
		spdlog::warn("vision71.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for vision71.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));
}

void WidescreenFix()
{
	if (eGameType == Game::DATFOD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionList1ScanResult = Memory::PatternScan(exeModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 00 00 48 42 54 69 6D");
		if (ResolutionList1ScanResult)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionList1ScanResult, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList1ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList1ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList2ScanResult = Memory::PatternScan(exeModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 47 61 6D 65 44 61 74 61");
		if (ResolutionList2ScanResult)
		{
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(ResolutionList2ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList2ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList3ScanResult = Memory::PatternScan(exeModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 10 32 40 00 68 A4");
		if (ResolutionList3ScanResult)
		{
			spdlog::info("Resolution List 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList3ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(ResolutionList3ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList3ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList3ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList3ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList3ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList3ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList4ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 61 0B 36 3B 4B 69 6C 6C 4D 6F 6E 73 74 65 72 41 63 74");
		if (ResolutionList4ScanResult)
		{
			spdlog::info("Resolution List 4 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList4ScanResult - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionList4ScanResult, iCurrentResX);

			Memory::Write(ResolutionList4ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList4ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList4ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList4ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList4ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 4 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList5ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 53 61 6D 70 6C 65 20 67 61 6D 65 20 70 6C 75 67 69 6E 3A 49 6E 69 74 45 6E 67 69 6E 65 50 6C");
		if (ResolutionList5ScanResult)
		{
			spdlog::info("Resolution List 5 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList5ScanResult - (std::uint8_t*)pluginModule);
			
			Memory::Write(ResolutionList5ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList5ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList5ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList5ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList5ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList5ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList6ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 53 6F 75 6E 64 73 00 00 2E 2E 2E 66 61 69 6C 65 64 21 00 00 4C");
		if (ResolutionList6ScanResult)
		{
			spdlog::info("Resolution List 6 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList6ScanResult - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionList6ScanResult, iCurrentResX);

			Memory::Write(ResolutionList6ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList6ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList6ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList6ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList6ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 6 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList7ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 4D 6F 6E 73 74 65 72 45 6E 74 69 74 79 00 00 00 70 61 72 74 69 63");
		if (ResolutionList7ScanResult)
		{
			spdlog::info("Resolution List 7 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList7ScanResult - (std::uint8_t*)pluginModule);
			
			Memory::Write(ResolutionList7ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList7ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList7ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList7ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList7ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList7ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 7 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList8ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 00 00 7A 43");
		if (ResolutionList8ScanResult)
		{
			spdlog::info("Resolution List 8 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList8ScanResult - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionList8ScanResult, iCurrentResX);

			Memory::Write(ResolutionList8ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList8ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList8ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList8ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList8ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 8 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList9ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 70 61 72 74");
		if (ResolutionList9ScanResult)
		{
			spdlog::info("Resolution List 9 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList9ScanResult - (std::uint8_t*)pluginModule);
			
			Memory::Write(ResolutionList9ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList9ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList9ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList9ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList9ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList9ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 9 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList10ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 40 67");
		if (ResolutionList10ScanResult)
		{
			spdlog::info("Resolution List 10 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList10ScanResult - (std::uint8_t*)pluginModule);
			
			Memory::Write(ResolutionList10ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList10ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList10ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList10ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList10ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList10ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 10 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList11ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 48 6F");
		if (ResolutionList11ScanResult)
		{
			spdlog::info("Resolution List 11 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList11ScanResult - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionList11ScanResult, iCurrentResX);

			Memory::Write(ResolutionList11ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList11ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList11ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList11ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList11ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 11 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList12ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 61 0B 36 3B 41 74 74");
		if (ResolutionList12ScanResult)
		{
			spdlog::info("Resolution List 12 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList12ScanResult - (std::uint8_t*)pluginModule);
			
			Memory::Write(ResolutionList12ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList12ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList12ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList12ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList12ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList12ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 12 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList13ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 31 00 00 00");
		if (ResolutionList13ScanResult)
		{
			spdlog::info("Resolution List 13 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList13ScanResult - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionList13ScanResult, iCurrentResX);

			Memory::Write(ResolutionList13ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList13ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList13ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList13ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList13ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 13 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList14ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 44 45 41 54 48");
		if (ResolutionList14ScanResult)
		{
			spdlog::info("Resolution List 14 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList14ScanResult - (std::uint8_t*)pluginModule);
			
			Memory::Write(ResolutionList14ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList14ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList14ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList14ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList14ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList14ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 14 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList15ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 61 0B 36 3B 70 61 72 74 69 63");
		if (ResolutionList15ScanResult)
		{
			spdlog::info("Resolution List 15 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList15ScanResult - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionList15ScanResult, iCurrentResX);

			Memory::Write(ResolutionList15ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList15ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList15ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList15ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList15ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 15 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList16ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 41 49 4E 6F 64 65");
		if (ResolutionList16ScanResult)
		{
			spdlog::info("Resolution List 16 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList16ScanResult - (std::uint8_t*)pluginModule);
			
			Memory::Write(ResolutionList16ScanResult, iCurrentResX);
			
			Memory::Write(ResolutionList16ScanResult + 4, iCurrentResX);
			
			Memory::Write(ResolutionList16ScanResult + 8, iCurrentResX);
			
			Memory::Write(ResolutionList16ScanResult + 12, iCurrentResY);
			
			Memory::Write(ResolutionList16ScanResult + 16, iCurrentResY);
			
			Memory::Write(ResolutionList16ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 16 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList17ScanResult = Memory::PatternScan(pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 50 6C 61 79");
		if (ResolutionList17ScanResult)
		{
			spdlog::info("Resolution List 17 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionList17ScanResult - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionList17ScanResult, iCurrentResX);

			Memory::Write(ResolutionList17ScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionList17ScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionList17ScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionList17ScanResult + 16, iCurrentResY);

			Memory::Write(ResolutionList17ScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list 17 scan memory address.");
			return;
		}

		/*
		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 8E 90 00 00 00 50 51 B9 ?? ?? ?? ??");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is vision71.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x90);

				fCurrentCameraFOV = CalculateNewFOV(fOriginalCameraFOV) * fFOVFactor;
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
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