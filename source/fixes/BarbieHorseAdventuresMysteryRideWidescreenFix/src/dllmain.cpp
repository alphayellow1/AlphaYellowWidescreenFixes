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
#include <cmath> // For atan, tan
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
std::string sFixName = "BarbieHorseAdventuresMysteryRideWidescreenFix";
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixActive = true;

// Variables
uint32_t iCurrentResX = 0;
uint32_t iCurrentResY = 0;
float fNewAspectRatio;
float fNewCameraFOV;
float fFOVFactor;

// Game detection
enum class Game
{
	BHAMR,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BHAMR, {"Barbie Horse Adventures: Mystery Ride", "Barbie Horse.exe"}},
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
	inipp::get_value(ini.sections["WidescreenFix"], "Enabled", FixActive);
	spdlog_confparse(FixActive);

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

void Fix()
{
	if (eGameType == Game::BHAMR) {
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewCameraFOV = fFOVFactor * (0.5f * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)));

		std::uint8_t* BHAMR_ResolutionWidthScanResult = Memory::PatternScan(exeModule, "C0 80 02 00 00 7C");

		std::uint8_t* BHAMR_ResolutionHeightScanResult = Memory::PatternScan(exeModule, "C4 E0 01 00 00 7C");

		std::uint8_t* BHAMR_AspectRatioScan1Result = Memory::PatternScan(exeModule, "75 00 A3 F0 A8 76 00 68 AB AA AA 3F");

		std::uint8_t* BHAMR_AspectRatioScan2Result = Memory::PatternScan(exeModule, "8A F7 FF 83 C4 0C 68 AB AA AA 3F");

		std::uint8_t* BHAMR_AspectRatioScan3Result = Memory::PatternScan(exeModule, "26 F7 FF 83 C4 0C 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan4Result = Memory::PatternScan(exeModule, "A3 C4 31 77 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan5Result = Memory::PatternScan(exeModule, "D8 E9 98 00 00 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan6Result = Memory::PatternScan(exeModule, "04 A3 F0 A8 76 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan7Result = Memory::PatternScan(exeModule, "ED FF 83 C4 04 89 45 F8 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan8Result = Memory::PatternScan(exeModule, "15 F0 A8 76 00 89 15 C4 31 77 00 68 AB AA AA 3F");

		std::uint8_t* BHAMR_AspectRatioScan9Result = Memory::PatternScan(exeModule, "EC FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan10Result = Memory::PatternScan(exeModule, "04 8B 0D F0 A8 76 00 89 0D C4 31 77 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan11Result = Memory::PatternScan(exeModule, "D6 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan12Result = Memory::PatternScan(exeModule, "00 51 E8 58 B5 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_AspectRatioScan13Result = Memory::PatternScan(exeModule, "0D F0 A8 76 00 68 AB AA AA 3F");

		std::uint8_t* BHAMR_AspectRatioScan14Result = Memory::PatternScan(exeModule, "00 52 E8 9F 5C EB FF 83 C4 04 A3 C4 31 77 00 A1 C4 31 77 00 A3 F0 A8 76 00 68 AB AA AA 3F 68");

		std::uint8_t* BHAMR_CameraFOVScan1Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D 48");

		std::uint8_t* BHAMR_CameraFOVScan2Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 B5");

		std::uint8_t* BHAMR_CameraFOVScan3Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 7E");

		std::uint8_t* BHAMR_CameraFOVScan4Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 CE");

		std::uint8_t* BHAMR_CameraFOVScan5Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 45");

		std::uint8_t* BHAMR_CameraFOVScan6Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 8A");

		std::uint8_t* BHAMR_CameraFOVScan7Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 4D");

		std::uint8_t* BHAMR_CameraFOVScan8Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D C4");

		std::uint8_t* BHAMR_CameraFOVScan9Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 2A");

		std::uint8_t* BHAMR_CameraFOVScan10Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 97");

		std::uint8_t* BHAMR_CameraFOVScan11Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 0E");

		std::uint8_t* BHAMR_CameraFOVScan12Result = Memory::PatternScan(exeModule, "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 D2");

		std::uint8_t* BHAMR_CameraFOVScan13Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 6B");

		std::uint8_t* BHAMR_CameraFOVScan14Result = Memory::PatternScan(exeModule, "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 1A");

		Memory::Write(BHAMR_ResolutionWidthScanResult + 0x1, iCurrentResX);

		Memory::Write(BHAMR_ResolutionHeightScanResult + 0x1, iCurrentResY);

		Memory::Write(BHAMR_AspectRatioScan1Result + 0x8, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan2Result + 0x7, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan3Result + 0x7, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan4Result + 0x6, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan5Result + 0x7, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan6Result + 0x7, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan7Result + 0x9, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan8Result + 0xC, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan9Result + 0x17, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan10Result + 0xE, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan11Result + 0x18, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan12Result + 0x1C, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan13Result + 0x6, fNewAspectRatio);

		Memory::Write(BHAMR_AspectRatioScan14Result + 0x1A, fNewAspectRatio);

		Memory::Write(BHAMR_CameraFOVScan1Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan2Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan3Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan4Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan5Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan6Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan7Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan8Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan9Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan10Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan11Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan12Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan13Result + 0x1, fNewCameraFOV);

		Memory::Write(BHAMR_CameraFOVScan14Result + 0x1, fNewCameraFOV);
	}
}

DWORD __stdcall Main(void*)
{
	Logging();
	Configuration();
	if (DetectGame())
	{
		Fix();
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