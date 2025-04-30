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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "SinbadLegendOfTheSevenSeasWidescreenFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraFOV;

// Game detection
enum class Game
{
	SLOTSS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SLOTSS, {"Sinbad: The Legend of the Seven Seas", "sinbad.exe"}},
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

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds : [eax]
		fmul dword ptr ds : [fNewCameraFOV]
		fstp dword ptr ds : [eax]

		fld dword ptr ds : [eax + 0x4]
		fmul dword ptr ds : [fNewCameraFOV]
		fstp dword ptr ds : [eax + 0x4]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::SLOTSS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionList1ScanResult = Memory::PatternScan(exeModule, "B9 00 04 00 00 BA 00 03 00 00 EB 1B 83 F8 02 75 16 B9 00 05 00 00 BA C0 03 00 00 EB 0A B9 20 03 00 00 BA 58 02 00 00");
		if (ResolutionList1ScanResult)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionList1ScanResult + 1, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 6, iCurrentResY);

			Memory::Write(ResolutionList1ScanResult + 18, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 23, iCurrentResY);

			Memory::Write(ResolutionList1ScanResult + 30, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 35, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList2ScanResult = Memory::PatternScan(exeModule, "20 03 00 00 58 02 00 00 10 00 00 00 01 00 00 00 00 04 00 00 00 03 00 00 10 00 00 00 01 00 00 00 00 05 00 00 C0 03 00 00 10 00 00 00 01 00 00 00 20 03 00 00 58 02 00 00 20 00 00 00 01 00 00 00 00 04 00 00 00 03 00 00 20 00 00 00 01 00 00 00 00 05 00 00 C0 03 00 00 20 00 00 00 01 00 00 00 72 00 65 00 73 00 6F 00 6C 00 75 00 74 00 69 00 6F 00 6E 00 00 00 00 00 64 00 65 00 70 00 74 00");
		if (ResolutionList2ScanResult)
		{
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList2ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionList2ScanResult, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 4, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 16, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 20, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 32, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 36, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 48, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 52, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 64, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 68, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 80, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 84, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list 2 scan memory address.");
			return;
		}

		fNewCameraFOV = 0.75f * fNewAspectRatio * fFOVFactor;
		
		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 08 8B 50 04 8B 45 08 89 48 68 89 50 6C 8B 4D 08");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
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