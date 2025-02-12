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
#include <tlhelp32.h>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "DarkenedSkyeFOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fNewAspectRatio;
float fNewGameplayCameraFOV;
float fNewCutscenesCameraFOV;

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
	DS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DS, {"Darkened Skye", "Skye.exe"}},
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
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
	return false;
}

float CalculateNewGameplayFOV(float fCurrentFOV)
{
	return fFOVFactor * (2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio))));
}

float CalculateNewCutscenesFOV(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));
}

static SafetyHookMid GameplayCameraFOVInstructionHook{};

static void GameplayCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float fOriginalGameplayCameraFOV = 90.0f;

	fNewGameplayCameraFOV = CalculateNewGameplayFOV(fOriginalGameplayCameraFOV);

	_asm
	{
		push eax
		mov eax, dword ptr ds : [0x0054D014]
		cmp eax, fOriginalGameplayCameraFOV
		je ChangeFOV
		jmp OriginalCode

		ChangeFOV :
		mov eax, dword ptr ds : [fNewGameplayCameraFOV]
			mov dword ptr ds : [0x0054D014] , eax
			jmp OriginalCode

			OriginalCode :
		pop eax
			fld dword ptr ds : [0x0054D014]
	}
}

static SafetyHookMid CutscenesCameraFOVInstructionHook{};

static void CutscenesCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	constexpr float fOriginalCutscenesCameraFOV1 = 30.0000248f;

	constexpr float fOriginalCutscenesCameraFOV2 = 45.00003433f;

	constexpr float fOriginalCutscenesCameraFOV3 = 45.000038146973f;

	constexpr float fOriginalCutscenesCameraFOV4 = 53.00004196167f;

	constexpr float fOriginalCutscenesCameraFOV5 = 60.000049591064f;

	constexpr float fOriginalCutscenesCameraFOV6 = 65.000679016113f;

	float fNewCutscenesCameraFOV1 = CalculateNewCutscenesFOV(fOriginalCutscenesCameraFOV1);

	float fNewCutscenesCameraFOV2 = CalculateNewCutscenesFOV(fOriginalCutscenesCameraFOV2);

	float fNewCutscenesCameraFOV3 = CalculateNewCutscenesFOV(fOriginalCutscenesCameraFOV3);

	float fNewCutscenesCameraFOV4 = CalculateNewCutscenesFOV(fOriginalCutscenesCameraFOV4);

	float fNewCutscenesCameraFOV5 = CalculateNewCutscenesFOV(fOriginalCutscenesCameraFOV5);

	float fNewCutscenesCameraFOV6 = CalculateNewCutscenesFOV(fOriginalCutscenesCameraFOV6);

	_asm
	{
		push eax
		mov eax, dword ptr ds : [0x0054D014]
		cmp eax, fOriginalCutscenesCameraFOV1
		je FOV1

		Compare1 :
		cmp eax, fOriginalCutscenesCameraFOV2
			je FOV2

			Compare2 :
		cmp eax, fOriginalCutscenesCameraFOV3
			je FOV3

			Compare3 :
		cmp eax, fOriginalCutscenesCameraFOV4
			je FOV4

			Compare4 :
		cmp eax, fOriginalCutscenesCameraFOV5
			je FOV5

			Compare5 :
		cmp eax, fOriginalCutscenesCameraFOV6
			je FOV6
			jmp OriginalCode

			FOV1 :
		mov eax, dword ptr ds : [fNewCutscenesCameraFOV1]
			mov dword ptr ds : [0x0054D014] , eax
			jmp Compare1

			FOV2 :
		mov eax, dword ptr ds : [fNewCutscenesCameraFOV2]
			mov dword ptr ds : [0x0054D014] , eax
			jmp Compare2

			FOV3 :
		mov eax, dword ptr ds : [fNewCutscenesCameraFOV3]
			mov dword ptr ds : [0x0054D014] , eax
			jmp Compare3

			FOV4 :
		mov eax, dword ptr ds : [fNewCutscenesCameraFOV4]
			mov dword ptr ds : [0x0054D014] , eax
			jmp Compare4

			FOV5 :
		mov eax, dword ptr ds : [fNewCutscenesCameraFOV5]
			mov dword ptr ds : [0x0054D014] , eax
			jmp Compare5

			FOV6 :
		mov eax, dword ptr ds : [fNewCutscenesCameraFOV6]
			mov dword ptr ds : [0x0054D014] , eax
			jmp OriginalCode

			OriginalCode :
		pop eax
	}
}

void FOVFix()
{
	if (eGameType == Game::DS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* GameplayCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 14 D0 54 00 D8 1D 7C D1 4B 00");
		if (GameplayCameraFOVInstructionScanResult)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			GameplayCameraFOVInstructionHook = safetyhook::create_mid(GameplayCameraFOVInstructionScanResult, GameplayCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate gameplay camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CutscenesCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 14 D0 54 00 D9 1D 7C D1 4B 00");
		if (CutscenesCameraFOVInstructionScanResult)
		{
			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CutscenesCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CutscenesCameraFOVInstructionHook = safetyhook::create_mid(CutscenesCameraFOVInstructionScanResult, CutscenesCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate cutsenes camera FOV instruction memory address.");
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