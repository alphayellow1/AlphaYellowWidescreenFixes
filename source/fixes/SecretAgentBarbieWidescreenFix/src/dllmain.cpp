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
bool FixActive;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
uint8_t newWidthSmall;
uint8_t newWidthBig;
uint8_t newHeightSmall;
uint8_t newHeightBig;
float fNewCameraFOV;
float fFOVFactor;

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

void FOV()
{
	newWidthSmall = iCurrentResX % 256;

	newWidthBig = (iCurrentResX - newWidthSmall) / 256;

	newHeightSmall = iCurrentResY % 256;

	newHeightBig = (iCurrentResY - newHeightSmall) / 256;

	if (eGameType == Game::SAB && FixActive == true) {
		std::uint8_t* SAB_CameraFOVScanResult = Memory::PatternScan(exeModule, "8B 54 24 08 89 81 B0 00 00 00");
		if (SAB_CameraFOVScanResult) {
			spdlog::info("Camera HFOV: Address is {:s}+{:x}", sExeName.c_str(), SAB_CameraFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid BIT12DP_CameraHFOVMidHook{};
			BIT12DP_CameraHFOVMidHook = safetyhook::create_mid(SAB_CameraFOVScanResult,
				[](SafetyHookContext& ctx) {
				if (ctx.eax == std::bit_cast<uint32_t>(35.0f)) {
					ctx.eax = std::bit_cast<uint32_t>(fFOVFactor * (2.0f * RadToDeg(atanf(tanf(DegToRad(35.0f / 2.0f)) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / fOldAspectRatio)))));
				}
				else if (ctx.eax == std::bit_cast<uint32_t>(90.0f)) {
					ctx.eax = std::bit_cast<uint32_t>(fFOVFactor * (2.0f * RadToDeg(atanf(tanf(DegToRad(90.0f / 2.0f)) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / fOldAspectRatio)))));
				}
			});
		}

		std::uint8_t* SAB_ResolutionScanResult = Memory::PatternScan(exeModule, "8B 4C 24 04 8B 54 24 08 89 48 40 8B 4C 24 0C 89 50 44 8B 54 24 10 89 48 48 89 50 4C");

		Memory::PatchBytes(SAB_ResolutionScanResult + 0x8, "\xE9\x0E\x2A\x08\x00\x90\x90\x90\x90\x90\x90\x90\x90\x90", 14);

		std::uint8_t* SAB_CodecaveScanResult = Memory::PatternScan(exeModule, "E9 6E 2D FB FF 00 00 00 00 00 00 00 00 00 00");

		Memory::PatchBytes(SAB_CodecaveScanResult + 0x13, "\xC7\x40\x40\x80\x02\x00\x00\x8B\x4C\xE4\x0C\xC7\x40\x44\xE0\x01\x00\x00\x8B\x54\xE4\x10\xE9\xE0\xD5\xF7\xFF", 27);

		std::uint8_t* SAB_NewCodecaveScanResult = Memory::PatternScan(exeModule, "C7 40 40 ?? ?? ?? ?? 8B 4C E4 0C C7 40 44 ?? ?? ?? ?? 8B 54 E4 10 E9 E0 D5 F7 FF");

		Memory::Write(SAB_NewCodecaveScanResult + 0x3, newWidthSmall);

		Memory::Write(SAB_NewCodecaveScanResult + 0x4, newWidthBig);

		Memory::Write(SAB_NewCodecaveScanResult + 0xE, newHeightSmall);

		Memory::Write(SAB_NewCodecaveScanResult + 0xF, newHeightBig);
	}
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