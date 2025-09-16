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
HMODULE dllModule2;
HMODULE dllModule3;
HMODULE thisModule;

// Fix details
std::string sFixName = "GotchaExtremePaintballWidescreenFix";
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
int iCurrentResX;
int iCurrentResY;
float fHipfireFOVFactor;
float fZoomFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalHipfireFOV = 90.0f;

// Variables
float fAspectRatioScale;
float fNewAspectRatio;
float fCurrentCameraFOV;
float fNewCameraFOV;
float fNewHipfireFOV;
float fNewZoomFOV;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	GEP,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::GEP, {"Gotcha! Extreme Paintball", "Gotcha.exe"}},
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
		// Use std::filesystem::path operations for clarity
		std::filesystem::path logFilePath = sExePath / sLogFile;
		logger = spdlog::basic_logger_st(sFixName, logFilePath.string(), true);
		spdlog::set_default_logger(logger);
		spdlog::flush_on(spdlog::level::debug);
		spdlog::set_level(spdlog::level::debug); // Enable debug level logging

		spdlog::info("----------");
		spdlog::info("{:s} v{:s} loaded.", sFixName, sFixVersion);
		spdlog::info("----------");
		spdlog::info("Log file: {}", logFilePath.string());
		spdlog::info("----------");
		spdlog::info("Module Name: {:s}", sExeName);
		spdlog::info("Module Path: {:s}", sExePath.string());
		spdlog::info("Module Address: 0x{:X}", (uintptr_t)exeModule);
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
	std::filesystem::path configPath = sFixPath / sConfigFile;
	std::ifstream iniFile(configPath);
	if (!iniFile)
	{
		AllocConsole();
		FILE* dummy;
		freopen_s(&dummy, "CONOUT$", "w", stdout);
		std::cout << sFixName << " v" << sFixVersion << " loaded." << std::endl;
		std::cout << "ERROR: Could not locate config file." << std::endl;
		std::cout << "ERROR: Make sure " << sConfigFile << " is located in " << sFixPath.string() << std::endl;
		spdlog::shutdown();
		FreeLibraryAndExitThread(thisModule, 1);
	}
	else
	{
		spdlog::info("Config file: {}", configPath.string());
		ini.parse(iniFile);
	}

	// Parse config
	ini.strip_trailing_comments();
	spdlog::info("----------");

	// Load settings from ini
	inipp::get_value(ini.sections["WidescreenFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load new INI entries
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "HipfireFOVFactor", fHipfireFOVFactor);
	inipp::get_value(ini.sections["Settings"], "ZoomFOVFactor", fZoomFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fHipfireFOVFactor);
	spdlog_confparse(fZoomFOVFactor);

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

	while ((dllModule2 = GetModuleHandleA("vision71.dll")) == nullptr)
	{
		spdlog::warn("vision71.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for vision71.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while ((dllModule3 = GetModuleHandleA("vBase71.dll")) == nullptr)
	{
		spdlog::warn("vBase71.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for vBase71.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	return true;
}

void WidescreenFix()
{
	if (eGameType == Game::GEP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 00 05 00 00 C0 03 00 00 00 05 00 00 00 04 00 00 40 06 00 00 B0 04 00 00 10 00 00 00 20 00 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionListScanResult, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 4, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 16, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 20, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 24, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 28, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 32, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 36, iCurrentResY);

			Memory::Write(ResolutionListScanResult + 40, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 44, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list memory address.");
			return;
		}

		std::uint8_t* HipfireCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "B8 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 9E");
		if (HipfireCameraFOVInstructionScanResult)
		{
			spdlog::info("Hipfire Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), HipfireCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			fNewHipfireFOV = Maths::CalculateNewFOV_DegBased(fOriginalHipfireFOV, fAspectRatioScale) * fHipfireFOVFactor;

			Memory::Write(HipfireCameraFOVInstructionScanResult + 1, fNewHipfireFOV);
		}
		else
		{
			spdlog::error("Failed to locate hipfire camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* ZoomFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 80 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 8E");
		if (ZoomFOVInstructionScanResult)
		{
			spdlog::info("Zoom FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), ZoomFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ZoomFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ZoomCameraFOVInstructionMidHook{};
			
			ZoomCameraFOVInstructionMidHook = safetyhook::create_mid(ZoomFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = *reinterpret_cast<float*>(ctx.eax + 0x3A8);
					
				fNewZoomFOV = Maths::CalculateNewFOV_DegBased(fCurrentZoomFOV, fAspectRatioScale) / fZoomFOVFactor;
					
				ctx.eax = std::bit_cast<uintptr_t>(fNewZoomFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate zoom FOV instruction memory address.");
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
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH: {
		thisModule = hModule;
		HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
		if (mainHandle) {
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