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
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fDefaultCameraHFOV = 1.5707963705062866f;
constexpr float fDefaultCameraHFOV2 = 1.8849556446075f;
constexpr float fDefaultCameraVFOV = 1.1780972480773926f;
constexpr float fDefaultCameraVFOV2 = 1.4137166738510132f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraHFOV2;
float fNewCameraHFOV3;
float fNewCameraHFOV4;
float fNewCameraVFOV;
float fNewCameraVFOV2;
float fNewCameraVFOV3;
float fNewCameraVFOV4;
bool bInitializationFailed = false;

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

void FOVFix()
{
	if (eGameType == Game::SMAD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		Sleep(3000);

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B B0 38 01 00 00 89 B4 24 D0 00 00 00 8B B0 3C 01 00 00 89 B4 24 D4 00 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax + 0x138);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x13C);

				if ((Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV) && Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV2)) && (Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV / fAspectRatioScale) && Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV2 / fAspectRatioScale)))
				{
					fNewCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, fAspectRatioScale, fFOVFactor);
				}
				else
				{
					fNewCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, fAspectRatioScale);
				}

				ctx.esi = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionScanResult + 13, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 13, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x138);

				float& fCurrentCameraVFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x13C);

				if ((Maths::isClose(fCurrentCameraHFOV2, fDefaultCameraHFOV) && Maths::isClose(fCurrentCameraHFOV2, fDefaultCameraHFOV2)) && (Maths::isClose(fCurrentCameraVFOV2, fDefaultCameraVFOV / fAspectRatioScale) && Maths::isClose(fCurrentCameraVFOV2, fDefaultCameraVFOV2 / fAspectRatioScale)))
				{
					fNewCameraVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentCameraVFOV2 * fAspectRatioScale, fFOVFactor);
				}
				else
				{
					fNewCameraVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentCameraVFOV2 * fAspectRatioScale);
				}

				ctx.esi = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
		}
	}
}

DWORD __stdcall Main(void*)
{
	Logging();
	if (bInitializationFailed)
	{
		return FALSE;
	}

	Configuration();
	if (bInitializationFailed)
	{
		return FALSE;
	}

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
		thisModule = hModule;
		{
			HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
			if (mainHandle)
			{
				SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
				CloseHandle(mainHandle);
			}
			break;
		}
	case DLL_PROCESS_DETACH:
		if (!bInitializationFailed)
		{
			spdlog::info("DLL has been successfully unloaded.");
			spdlog::shutdown();
			// Hooks go out of scope and clean up automatically
		}
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}