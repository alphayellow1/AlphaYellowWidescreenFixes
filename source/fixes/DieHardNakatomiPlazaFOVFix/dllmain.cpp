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
#include <unordered_set>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "DieHardNakatomiPlazaFOVFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fTolerance = 0.000001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fCurrentZoomCameraHFOV;
float fCurrentZoomCameraVFOV;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fFOVFactor;
float fAspectRatioScale;
static float fUnderwaterCheckValue;

// Game detection
enum class Game
{
	DHNP,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DHNP, {"Die Hard: Nakatomi Plaza", "Lithtech.exe"}},
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

float CalculateNewHFOVWithoutFOVFactor(float fCurrentHFOV)
{
	return 2.0f * atanf((tanf(fCurrentHFOV / 2.0f)) * fAspectRatioScale);
}

float CalculateNewHFOVWithFOVFactor(float fCurrentHFOV)
{
	return 2.0f * atanf((fFOVFactor * tanf(fCurrentHFOV / 2.0f)) * fAspectRatioScale);
}

float CalculateNewVFOVWithoutFOVFactor(float fCurrentVFOV)
{
	return 2.0f * atanf(tanf(fCurrentVFOV / 2.0f));
}

float CalculateNewVFOVWithFOVFactor(float fCurrentVFOV)
{
	return 2.0f * atanf(fFOVFactor * tanf(fCurrentVFOV / 2.0f));
}

bool bIsDefaultHFOV(float fCurrentHFOV)
{
	return fabsf(fCurrentHFOV - 1.5707963705062866f) < fTolerance;
}

bool bIsDefaultVFOV(float fCurrentVFOV)
{
	return fabsf(fCurrentVFOV - 1.1780972480773926f) < fTolerance;
}

bool bIsCroppedVFOV(float fCurrentVFOV)
{
	return fabsf(fCurrentVFOV - (1.1780972480773926f / fAspectRatioScale)) < fTolerance;
}

bool bIsZoomHFOV(float fCurrentHFOV)
{
	return fabsf(fCurrentHFOV - 0.4363323152065277f) < fTolerance;
}

bool bIsZoomVFOV(float fCurrentVFOV)
{
	return fabsf(fCurrentVFOV - (0.3272492289543152f / fAspectRatioScale)) < fTolerance;
}

bool bIsDefaultMenuHFOV(float fCurrentHFOV)
{
	return fabsf(fCurrentHFOV - 1.570000052f) < fTolerance;
}

bool bIsDefaultMenuVFOV(float fCurrentHFOV)
{
	return fabsf(fCurrentHFOV - 1.169999957f) < fTolerance;
}

void FOVFix()
{
	if (eGameType == Game::DHNP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

	    std::uint8_t* UnderwaterCheckInstructionScanResult = Memory::PatternScan(exeModule, "A3 BC 64 4E 00 8B 46 3C D9 05 BC 64 4E 00");
		if (UnderwaterCheckInstructionScanResult)
		{
			spdlog::info("Underwater Check Instruction: Address is {:s}+{:x}", sExeName.c_str(), UnderwaterCheckInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid UnderwaterCheckInstructionMidHook{};

			UnderwaterCheckInstructionMidHook = safetyhook::create_mid(UnderwaterCheckInstructionScanResult, [](SafetyHookContext& ctx)
			{
				fUnderwaterCheckValue = std::bit_cast<float>(ctx.eax);
			});
		}
		else
		{
			spdlog::error("Failed to locate underwater check instruction memory address.");
			return;
		}
		
		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 9C 01 00 00 89 45 C0");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.ecx + 0x198);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.ecx + 0x19C);				

				if (fUnderwaterCheckValue == 1.0f) // Above water
				{
					if (bIsDefaultHFOV(fCurrentCameraHFOV) && (bIsDefaultVFOV(fCurrentCameraVFOV) || bIsCroppedVFOV(fCurrentCameraVFOV)))
					{
						fNewCameraVFOV = CalculateNewVFOVWithFOVFactor(1.1780972480773926f);
					}
					else if (bIsZoomHFOV(fCurrentCameraHFOV) && bIsZoomVFOV(fCurrentCameraVFOV))
					{
						fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);
					}
					else if (bIsDefaultMenuHFOV(fCurrentCameraHFOV) && bIsDefaultMenuVFOV(fCurrentCameraVFOV))
					{
						fNewCameraVFOV = fCurrentCameraVFOV;
					}
					else
					{
						fNewCameraVFOV = fCurrentCameraVFOV;
					}
				}
				else if (fUnderwaterCheckValue == 0.007843137719f) // Underwater
				{
					fNewCameraVFOV = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV * fAspectRatioScale); // Underwater VFOVs (Since it's Vert- by default)
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire/cutscenes VFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 98 01 00 00 89 45 BC");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x198);

				float& fCurrentCameraVFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x19C);

				if (fUnderwaterCheckValue == 1.0f) // Above water
				{
					if (bIsDefaultHFOV(fCurrentCameraHFOV2) && (bIsDefaultVFOV(fCurrentCameraVFOV2) || bIsCroppedVFOV(fCurrentCameraVFOV2)))
					{
						fNewCameraHFOV = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV2);
					}
					else if (bIsZoomHFOV(fCurrentCameraHFOV2) && bIsZoomVFOV(fCurrentCameraVFOV2))
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV2);
					}
					else if (bIsDefaultMenuHFOV(fCurrentCameraHFOV2) && bIsDefaultMenuVFOV(fCurrentCameraVFOV2))
					{
						fNewCameraHFOV = fCurrentCameraHFOV2;
					}
					else
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV2);
					}
				}
				else if (fUnderwaterCheckValue == 0.007843137719f) // Underwater
				{
					fNewCameraHFOV = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV2);
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire/cutscenes HFOV instruction memory address.");
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