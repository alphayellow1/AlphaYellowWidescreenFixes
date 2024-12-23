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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixFOV = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
float newAspectRatio;
float newMenuFOV;
float value1;
float value2;
float value3;
float value4;

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

const GameInfo* game = nullptr;
Game eGameType = Game::Unknown;

void Logging()
{
	// Get path to DLL
	WCHAR dllPath[_MAX_PATH] = { 0 };
	GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
	sFixPename();

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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixFOV);
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
	std::uint8_t* AER_HFOVScanResult = Memory::PatternScan(exeModule, "D9 40 3C D8 49 28");
	if (AER_HFOVScanResult)
	{
		spdlog::info("Horizontal FOV: Address is {:s}+{:x}", sExeName.c_str(), AER_HFOVScanResult - (std::uint8_t*)exeModule);
		static SafetyHookMid AER_HFOVMidHook{};
		AER_HFOVMidHook = safetyhook::create_mid(AER_HFOVScanResult,
			[](SafetyHookContext& ctx) {
			*reinterpret_cast<float*>(ctx.eax + 0x3C) = 0.5f * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f));
		});
	}

	std::uint8_t* AER_VFOVScanResult = Memory::PatternScan(exeModule, "D9 42 40 D8 48 28");
	if (AER_VFOVScanResult)
	{
		spdlog::info("Vertical FOV: Address is {:s}+{:x}", sExeName.c_str(), AER_VFOVScanResult - (std::uint8_t*)exeModule);
		static SafetyHookMid AER_VFOVMidHook{};
		AER_VFOVMidHook = safetyhook::create_mid(AER_VFOVScanResult,
			[](SafetyHookContext& ctx) {
			*reinterpret_cast<float*>(ctx.edx + 0x40) = 0.375f * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f));
		});
	}

	std::uint8_t* AER_MenuAspectRatioAndFOVScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 10 68 6C 34 58 00 E8 00 1F 00 00 83 C4 04");
	spdlog::info("Menu Aspect Ratio & FOV: Address is {:s}+{:x}", sExeName.c_str(), AER_MenuAspectRatioAndFOVScanResult - (std::uint8_t*)exeModule);

	std::uint8_t* AER_MenuAspectRatioAndFOV2ScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A 00 68 AC CD BC 00 E8 2C 8F F8 FF 83 C4 10 8B 0D A0 7A 57 00");
	spdlog::info("Menu Aspect Ratio & FOV 2: Address is {:s}+{:x}", sExeName.c_str(), AER_MenuAspectRatioAndFOV2ScanResult - (std::uint8_t*)exeModule);

	std::uint8_t* AER_Value1ScanResult = Memory::PatternScan(exeModule, "10 8B 45 08 C7 40 28 CD CC CC 3F 6A 00 8B");

	std::uint8_t* AER_Value2ScanResult = Memory::PatternScan(exeModule, "BF 00 C7 45 F4 00 00 80 3F E9 A0 01");

	std::uint8_t* AER_Value3ScanResult = Memory::PatternScan(exeModule, "BF 00 C7 45 F4 00 00 80 3F C7 05 30");

	std::uint8_t* AER_Value4ScanResult = Memory::PatternScan(exeModule, "00 00 00 70 42 00 00 00 00 F4 1D DB 43 F8 B3");

	newAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

	newMenuFOV = 0.5f * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f));

	value1 = 1.6f * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f));

	value2 = 1.0f * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f));

	value3 = 1.0f * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f));

	value4 = 2.0f * RadToDeg(atanf(tanf(DegToRad(60.0f / 2.0f)) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio)));

	Memory::Write(AER_MenuAspectRatioAndFOVScanResult + 0x1, newAspectRatio);

	Memory::Write(AER_MenuAspectRatioAndFOVScanResult + 0x6, newMenuFOV);

	Memory::Write(AER_MenuAspectRatioAndFOV2ScanResult + 0x1, newAspectRatio);

	Memory::Write(AER_MenuAspectRatioAndFOV2ScanResult + 0x6, newMenuFOV);

	Memory::Write(AER_Value1ScanResult + 0x7, value1);

	Memory::Write(AER_Value2ScanResult + 0x5, value2);

	Memory::Write(AER_Value3ScanResult + 0x5, value3);

	Memory::Write(AER_Value4ScanResult + 0x1, value4);
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
