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
std::string sFixName = "MechWarrior3FOVFix";
std::string sFixVersion = "1.4";
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
float fNewCameraHFOV;
float fNewAspectRatio;

// Function to convert degrees to radians
static float DegToRad(float degrees)
{
	return degrees * (fPi / 180.0f);
}

// Function to convert radians to degrees
static float RadToDeg(float radians)
{
	return radians * (180.0f / fPi);
}

// Game detection
enum class Game
{
	MW3,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MW3, {"MechWarrior 3", "Mech3.exe"}},
};

const GameInfo* game = nullptr;
Game eGameType = Game::Unknown;

static void Logging()
{
	// Get path to DLL
	WCHAR dllPath[_MAX_PATH] = { 0 };
	GetModuleFileNameW(thisModule, dllPath, MAX_PATH);
	sFixPath = dllPath;
	sFixPath = sFixPath.remove_filename();

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

static void Configuration()
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

static bool DetectGame()
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

float CalculateNewHFOV(float fCurrentFOV)
{
	return (2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)))) / 160.0f;
}

static SafetyHookMid CameraHFOVInstruction1Hook{};

static void CameraHFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	fNewCameraHFOV = CalculateNewHFOV(80.0f);

	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

static SafetyHookMid CameraHFOVInstruction2Hook{};

static void CameraHFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	fNewCameraHFOV = CalculateNewHFOV(80.0f);

	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

static SafetyHookMid CameraHFOVInstruction3Hook{};

static void CameraHFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	fNewCameraHFOV = CalculateNewHFOV(80.0f);

	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

static SafetyHookMid CameraHFOVInstruction4Hook{};

static void CameraHFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	fNewCameraHFOV = CalculateNewHFOV(80.0f);

	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

static SafetyHookMid CameraHFOVInstruction5Hook{};

static void CameraHFOVInstruction5MidHook(SafetyHookContext& ctx)
{
	fNewCameraHFOV = CalculateNewHFOV(80.0f);

	_asm
	{
		fmul dword ptr ds : [fNewCameraHFOV]
	}
}

static void FOVFix()
{
	if (eGameType == Game::MW3 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraHFOVInstruction1And2Result = Memory::PatternScan(exeModule, "D9 82 E8 00 00 00 D8 0D ?? ?? ?? ?? D9 C9 D8 0D ?? ?? ?? ?? D9 C9");
		if (CameraHFOVInstruction1And2Result)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction1And2Result + 0x6 - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction1And2Result + 0xE - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction1And2Result + 0x6, "\x90\x90\x90\x90\x90\x90", 6);

			Memory::PatchBytes(CameraHFOVInstruction1And2Result + 0xE, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraHFOVInstruction1And2Result + 0xC, CameraHFOVInstruction1MidHook);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstruction1And2Result + 0x14, CameraHFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 1 and 2 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction3Result = Memory::PatternScan(exeModule, "D9 C9 D8 0D ?? ?? ?? ?? D9 44 24 10 D9 CA");
		if (CameraHFOVInstruction3Result)
		{
			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction3Result + 0x2 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction3Result + 0x2, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstruction3Result + 0x8, CameraHFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction4Result = Memory::PatternScan(exeModule, "D9 86 E8 00 00 00 D8 0D ?? ?? ?? ?? 89 BE F8 00 00 00");
		if (CameraHFOVInstruction4Result)
		{
			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction4Result + 0x6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction4Result + 0x6, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraHFOVInstruction4Result + 0xC, CameraHFOVInstruction4MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction5Result = Memory::PatternScan(exeModule, "D9 86 EC 00 00 00 D8 0D ?? ?? ?? ?? D9 F2");
		if (CameraHFOVInstruction5Result)
		{
			spdlog::info("Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction5Result + 0x6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction5Result + 0x6, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction5Hook = safetyhook::create_mid(CameraHFOVInstruction5Result + 0xC, CameraHFOVInstruction5MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 5 memory address.");
			return;
		}
	}
}

static DWORD __stdcall Main(void*)
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
