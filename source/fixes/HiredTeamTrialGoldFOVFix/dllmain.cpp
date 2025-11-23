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
HMODULE dllModule2 = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "HiredTeamTrialGoldFOVFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHipfireCameraFOV;
float fNewZoomCameraFOV;
float fNewMainMenuCameraFOV;
uint8_t* HipfireCameraFOVAddress;
uint8_t* ZoomCameraFOVAddress;
uint8_t* MainMenuCameraFOVAddress;

// Game detection
enum class Game
{
	HTTG,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::HTTG, {"Hired Team: Trial Gold", "Shine.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load new INI entries
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

	while ((dllModule2 = GetModuleHandleA("ShineEng.dll")) == nullptr)
	{
		spdlog::warn("ShineEng.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for ShineEng.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid HipfireCameraFOVInstructionHook{};
static SafetyHookMid MainMenuCameraFOVInstructionHook{};
static SafetyHookMid ZoomCameraFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::HTTG && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* HipfireCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 01 74 0A DD D8");
		if (HipfireCameraFOVInstructionScanResult)
		{
			spdlog::info("Hipfire Camera FOV Instruction: Address is ShineEng.dll+{:x}", HipfireCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			HipfireCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(HipfireCameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(HipfireCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			HipfireCameraFOVInstructionHook = safetyhook::create_mid(HipfireCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireCameraFOV = *reinterpret_cast<float*>(HipfireCameraFOVAddress);

				fNewHipfireCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentHipfireCameraFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewHipfireCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* ZoomCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "A1 ?? ?? ?? ?? 89 44 24 00 D9 44 24 00 D8 1D ?? ?? ?? ?? DF E0 F6 C4 01 74 0A");
		if (ZoomCameraFOVInstructionScanResult)
		{
			spdlog::info("Zoom Camera FOV Instruction: Address is ShineEng.dll+{:x}", ZoomCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			ZoomCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(ZoomCameraFOVInstructionScanResult + 1, Memory::PointerMode::Absolute);
			
			Memory::PatchBytes(ZoomCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90", 5);			
			
			ZoomCameraFOVInstructionHook = safetyhook::create_mid(ZoomCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomCameraFOV = *reinterpret_cast<float*>(ZoomCameraFOVAddress);

				fNewZoomCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentZoomCameraFOV, fAspectRatioScale);

				ctx.eax = std::bit_cast<uintptr_t>(fNewZoomCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate zoom camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* MainMenuCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 44 24 38 DB 44 24 38 D9 5C 24 50 DB 45 0C DB 44 24 10 D9 44 24 50");
		if (MainMenuCameraFOVInstructionScanResult)
		{
			spdlog::info("Main Menu Camera FOV Instruction: Address is ShineEng.dll+{:x}", MainMenuCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			MainMenuCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(MainMenuCameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);
			
			Memory::PatchBytes(MainMenuCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			MainMenuCameraFOVInstructionHook = safetyhook::create_mid(MainMenuCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentMainMenuCameraFOV = *reinterpret_cast<float*>(MainMenuCameraFOVAddress);

				fNewMainMenuCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentMainMenuCameraFOV, fAspectRatioScale);

				FPU::FMUL(fNewMainMenuCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate main menu camera FOV instruction memory address.");
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