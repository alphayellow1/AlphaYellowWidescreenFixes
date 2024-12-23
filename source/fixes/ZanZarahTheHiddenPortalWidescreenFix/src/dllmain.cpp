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

HMODULE exeModule;
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixFOV = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

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
	inipp::get_value(ini.sections["FOV"], "Enabled", FixFOV);
	spdlog_confparse(FixFOV);

	// Load resolution from ini
	inipp::get_value(ini.sections["Resolution"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Resolution"], "Height", iCurrentResY);
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
	exeModule = GetModuleHandleA("zanthp.exe");

	while (!exeModule)
	{
		spdlog::warn("Waiting for zanthp.exe to load...");
		Sleep(1000);
	}

	spdlog::info("Successfully obtained handle for zanthp.exe: 0x{:X}", reinterpret_cast<uintptr_t>(exeModule));

	return true;
}

void FOV()
{
	float newValue1 = 0.75f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	float newValue2 = 0.65f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	float newValue3 = 0.71f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	float newValue4 = 0.72f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	float newValue5 = 0.68f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	float newValue6 = 0.725f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	float newValue7 = 0.685f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	float newValue8 = 0.69f * ((4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / iCurrentResY));

	std::uint8_t* Pattern1ScanResult = Memory::PatternScan(exeModule, "3B 05 ?? ?? ?? ?? 74 4E 83 4D 10 FF");

	std::uint8_t* Pattern2ScanResult = Memory::PatternScan(exeModule, "C4 F3 A5 8D 73 2C 8B CE E8 67 5C");

	std::uint8_t* Pattern3ScanResult = Memory::PatternScan(exeModule, "8B B0 A0 5C 5A 00 89 71 08 8B B0 A4 5C 5A 00 89 71 0C 8B 80 A8 5C 5A 00 89 41 10 89 51 14 5E");

	std::uint8_t* Pattern4ScanResult = Memory::PatternScan(exeModule, "33 C9 8B 30 89 14 24 89 74 24 04 8B 70 04 89 74 24 08 8B 40 08 89 4C 24 14 89 4C 24 10");

	std::uint8_t* Pattern5ScanResult = Memory::PatternScan(exeModule, "F9 FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");

	std::uint8_t* Pattern6ScanResult = Memory::PatternScan(exeModule, "DA E1 42 00 00 00 40 3F");

	std::uint8_t* Pattern7ScanResult = Memory::PatternScan(exeModule, "7D 3F 66 66 26 3F 00 00");

	std::uint8_t* Pattern8ScanResult = Memory::PatternScan(exeModule, "66 66 A6 3F 00 00 00 00 00 00 40 3F 00 00 80 3F");

	std::uint8_t* Pattern9ScanResult = Memory::PatternScan(exeModule, "00 00 C0 BF AB AA AA 3E 66 66 E6 3F 8F C2 35 3F");

	std::uint8_t* Pattern10ScanResult = Memory::PatternScan(exeModule, "EC 51 38 3F 7B 14 2E 3F C3 F5 08 3F A6 9B C4 3B 0A D7 23 3B CD CC 0C 3F 9A 99 39 3F 29 5C 2F 3F AD AC EF B7 17 B7 D1 37 7B 14 0E 3F 00 00 C0 3E E1 7A 14 3E D7 A3 30 3F");

	Memory::PatchBytes(Pattern1ScanResult + 0x6, "\x90\x90", 2);

	Memory::PatchBytes(Pattern2ScanResult + 0x3, "\xE9\x0E\x2C\x19\x00", 5);

	Memory::PatchBytes(Pattern3ScanResult, "\xE9\x87\xAA\x18\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 31);

	Memory::PatchBytes(Pattern4ScanResult, "\x59\x59\x59\x68\x38\x04\x00\x00\x68\x80\x07\x00\x00\x52\x33\xC9\x89\x4C\x24\x14\x89\x4C\x24\x10\xB8\x20\x00\x00\x00", 29);

	Memory::PatchBytes(Pattern5ScanResult, "\xF9\xFF\x00\x8D\x73\x2C\x8B\xCE\xC7\x44\x24\x08\x80\x07\x00\x00\xC7\x44\x24\x0C\x38\x04\x00\x00\xE9\xD8\xD3\xE6\xFF\x00\x00\x00\xC7\x41\x08\x80\x07\x00\x00\xC7\x41\x0C\x38\x04\x00\x00\xC7\x41\x10\x20\x00\x00\x00\xC7\x41\x14\x00\x00\x00\x00\x5E\xC2\x04", 63);

	Memory::Write(Pattern6ScanResult + 0x4, newValue1);

	Memory::Write(Pattern7ScanResult + 0x2, newValue2);

	Memory::Write(Pattern8ScanResult + 0x8, newValue1);

	Memory::Write(Pattern9ScanResult + 0xC, newValue3);

	Memory::Write(Pattern10ScanResult, newValue4);

	Memory::Write(Pattern10ScanResult + 0x4, newValue5);

	Memory::Write(Pattern10ScanResult + 0x12, newValue6);

	Memory::Write(Pattern10ScanResult + 0x1C, newValue7);

	Memory::Write(Pattern10ScanResult + 0x34, newValue8);

	std::uint8_t* Pattern11ScanResult = Memory::PatternScan(exeModule, "F9 FF 00 8D 73 2C 8B CE C7 44 24 08 80 07 00 00 C7 44 24 0C 38 04 00 00 E9 D8 D3 E6 FF 00 00 00 C7 41 08 80 07 00 00 C7 41 0C 38 04 00 00 C7 41 10 20 00 00 00 C7 41 14 00 00 00 00 5E C2 04");

	Memory::Write(Pattern11ScanResult + 0xC, iCurrentResX);

	Memory::Write(Pattern11ScanResult + 0x14, iCurrentResY);
}

DWORD __stdcall Main(void*)
{
	Logging();
	Configuration();
	if (DetectGame())
	{
		FOV();
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