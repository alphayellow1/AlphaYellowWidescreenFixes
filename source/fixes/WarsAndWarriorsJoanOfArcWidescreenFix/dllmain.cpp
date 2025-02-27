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
std::string sFixName = "WarsAndWarriorsJoanOfArcWidescreenFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;

// Game detection
enum class Game
{
	WAWJOA,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WAWJOA, {"Wars and Warriors: Joan of Arc", "joan.exe"}},
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
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}
}

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * atanf(tanf(fCurrentFOV / 2.0f) * (fNewAspectRatio / fOldAspectRatio));
}

void WidescreenFix()
{
	if (eGameType == Game::WAWJOA && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionScan1Result = Memory::PatternScan(exeModule, "00 00 00 04 00 00 8B 4D FC C7 81 86 00 00 00 00 03 00 00 8B 55 FC");
		if (ResolutionScan1Result)
		{
			spdlog::info("Resolution Scan 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScan1Result - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScan1Result + 0x2, iCurrentResX);

			Memory::Write(ResolutionScan1Result + 0xF, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan 1 memory address.");
			return;
		}

		std::uint8_t* ResolutionScan2Result = Memory::PatternScan(exeModule, "C7 45 F4 00 04 00 00 C7 45 F0 00 03 00 00 C7 45 EC 00");
		if (ResolutionScan2Result)
		{
			spdlog::info("Resolution Scan 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScan2Result - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScan2Result + 0x3, iCurrentResX);

			Memory::Write(ResolutionScan2Result + 0xA, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan 2 memory address.");
			return;
		}

		std::uint8_t* ResolutionScan3Result = Memory::PatternScan(exeModule, "23 34 00 00 00 04 8B 45 FC 66 C7 80 25 34 00 00 00 03 EB 18");
		if (ResolutionScan3Result)
		{
			spdlog::info("Resolution Scan 3: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScan3Result - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScan3Result + 0x4, iCurrentResX);

			Memory::Write(ResolutionScan3Result + 0x10, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan 3 memory address.");
			return;
		}

		std::uint8_t* ResolutionScan4Result = Memory::PatternScan(exeModule, "23 34 00 00 00 04 8B 55 FC 66 C7 82 25 34 00 00 00 03 C7 45 F8 00");
		if (ResolutionScan4Result)
		{
			spdlog::info("Resolution Scan 4: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScan4Result - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScan4Result + 0x4, iCurrentResX);

			Memory::Write(ResolutionScan4Result + 0x10, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan 4 memory address.");
			return;
		}

		std::uint8_t* ResolutionScan5Result = Memory::PatternScan(exeModule, "00 00 04 00 00 00 03 00 00 10 00 00 00 00 00 00 00 00 00 00 00 FF 03 00 00 FF 02 00 00 00 00 00 00 64 00 00 00 00 00 00 00 00 00 00 00 FF 03 00 00 FF 02 00 00 00 04 00 00 00 03 00 00");
		if (ResolutionScan5Result)
		{
			spdlog::info("Resolution Scan 5: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScan5Result - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScan5Result + 0x1, iCurrentResX);

			Memory::Write(ResolutionScan5Result + 0x5, iCurrentResY);

			Memory::Write(ResolutionScan5Result + 0x35, iCurrentResX);

			Memory::Write(ResolutionScan5Result + 0x39, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan 5 memory address.");
			return;
		}

		/*
		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 45 0C 8B 45 08 D8 75 FC");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			static float fLastModifiedFOV = 0.0f;

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOVValue = *reinterpret_cast<float*>(ctx.ebp + 0xC);

				if (fCurrentFOVValue == 0.9238795042f)
				{
					fCurrentFOVValue *= (1.0f / fFOVFactor);
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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