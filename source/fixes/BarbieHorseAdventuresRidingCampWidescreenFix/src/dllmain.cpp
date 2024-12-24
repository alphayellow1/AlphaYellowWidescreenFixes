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
#include <cmath> // For atan, tan
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "BarbieHorseAdventuresRidingCampWidescreenFix";
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;
constexpr float originalCameraFOV = 1.169370532f;

// Ini variables
bool FixActive = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
float fFOVFactor;
float fNewAspectRatio;

// Game detection
enum class Game
{
	BHARC,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BHARC, {"Barbie Horse Adventures: Riding Camp", "PXGameStudioRuntime2008.exe"}},
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
	if (eGameType == Game::BHARC) {
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* BHARC_ResolutionWidthScanResult = Memory::PatternScan(exeModule, "78 05 00 00 89 1D E8 C5 5F 00 B8 00 04 00 00 B9");

		std::uint8_t* BHARC_ResolutionHeightScanResult = Memory::PatternScan(exeModule, "B9 00 03 00 00 73 08 8D 41 20 B9");

		Memory::Write(BHARC_ResolutionWidthScanResult + 0xB, iCurrentResX);

		Memory::Write(BHARC_ResolutionHeightScanResult + 0x1, iCurrentResY);

		std::uint8_t* BHARC_AspectRatioScanResult = Memory::PatternScan(exeModule, "D9 5C 24 08 D9 40 A4 D8 4C 24 28");
		if (BHARC_AspectRatioScanResult) {
			spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), BHARC_AspectRatioScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid BHARC_AspectRatioMidHook{};
			BHARC_AspectRatioMidHook = safetyhook::create_mid(BHARC_AspectRatioScanResult,
				[](SafetyHookContext& ctx) {
				*reinterpret_cast<float*>(ctx.eax - 0x5C) = fNewAspectRatio;
			});
		}

		std::uint8_t* BHARC_CameraFOVScanResult = Memory::PatternScan(exeModule, "D9 5C 24 04 D9 40 A0 D9 1C 24");
		if (BHARC_CameraFOVScanResult) {
			spdlog::info("Camera FOV: Address is {:s}+{:x}", sExeName.c_str(), BHARC_CameraFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid BHARC_CameraFOVMidHook{};
			BHARC_CameraFOVMidHook = safetyhook::create_mid(BHARC_CameraFOVScanResult,
				[](SafetyHookContext& ctx) {
				*reinterpret_cast<float*>(ctx.eax - 0x60) = originalCameraFOV * fFOVFactor;
			});
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