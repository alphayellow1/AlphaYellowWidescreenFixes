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
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "ShogoMobileArmorDivisionFOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float epsilon = 0.00001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;

// Game detection
enum class Game
{
	SMAD,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SMAD, {"Shogo: Mobile Armor Division", "Client.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

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

void FOVFix()
{
	if (eGameType == Game::SMAD && bFixActive == true)
	{
		std::uint8_t* HipfireAndCutscenesHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B B0 38 01 00 00 89 B4 24 D0 00 00 00");
		if (HipfireAndCutscenesHFOVInstructionScanResult)
		{
			spdlog::info("Hipfire and Cutscenes HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), HipfireAndCutscenesHFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid HipfireAndCutscenesHFOVInstructionMidHook{};
			HipfireAndCutscenesHFOVInstructionMidHook = safetyhook::create_mid(HipfireAndCutscenesHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (*reinterpret_cast<float*>(ctx.eax + 0x138) == 1.5707963705062866f || *reinterpret_cast<float*>(ctx.eax + 0x138) == 1.8849556446075f)
				{
					*reinterpret_cast<float*>(ctx.eax + 0x138) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.eax + 0x138) / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio));
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire and cutscenes camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* HipfireAndCutscenesVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B B0 3C 01 00 00 89 B4 24 D4 00 00 00");
		if (HipfireAndCutscenesVFOVInstructionScanResult)
		{
			spdlog::info("Hipfire and Cutscenes VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), HipfireAndCutscenesVFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid HipfireAndCutscenesVFOVInstructionMidHook{};
			HipfireAndCutscenesVFOVInstructionMidHook = safetyhook::create_mid(HipfireAndCutscenesVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (0.78539818525314334f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
				{
					*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314334f;
				}
				else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.1780972480773926f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
				{
					*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f;
				}
				else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.4137166738510132f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
				{
					*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.4137166738510132f;
				}

				if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 5.0f / 4.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.1297270059586f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (0.75315129756927f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 3.0f / 2.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.2662309408188f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (0.84415400028229f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 16.0f / 10.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.3140871524811f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (0.87605810165405f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 15.0f / 9.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.344083070755f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (0.89605540037155f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 16.0f / 9.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.3909428119659f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (0.92729520797729f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 1.85f / 1.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.4194481372833f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (0.9462987780571f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 2560.0f / 1080.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.587610244751f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.058406829834f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 3440.0f / 1440.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.592588186264f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.0617254972458f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 2.39f / 1.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.5928848981857f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.0619232654572f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 3840.0f / 1600.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.5955467224121f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.0636978149414f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 2.76f / 1.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.6811498403549f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.1207666397095f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 32.0f / 10.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.7640079259872f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.1760053634644f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 32.0f / 9.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.8180385828018f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.2120257616043f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 15.0f / 4.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.8437712192535f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.2291808128357f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 12.0f / 3.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.8735685348511f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.2490457296371f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 48.0f / 10.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.94977414608f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.2998495101929f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 45.0f / 9.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.9652907848358f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.3101938962936f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
				else if (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY) == 48.0f / 9.0f)
				{
					if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.9887266159058f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 1.1780972480773926f; // GAMEPLAY VFOV
					}
					else if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x13C) - (1.32581782341f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.eax + 0x13C) = 0.78539818525314f;   // CUTSCENE VFOV
					}
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire and cutscenes camera VFOV instruction memory address.");
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