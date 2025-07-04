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
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "EliteForcesWW2IwoJimaFOVFix";
std::string sFixVersion = "1.6";
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
constexpr float fTolerance = 0.00001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;
static uint16_t iIsUnderwater;

// Game detection
enum class Game
{
	EFWW2IJ,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::EFWW2IJ, {"Elite Forces: WWII - Iwo Jima", "client.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("cshell.dll")) == nullptr)
	{
		spdlog::warn("cshell.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for cshell.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
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

bool bIsCroppedVFOV(float fCurrentVFOV)
{
	return fabsf(fCurrentVFOV - (1.1780972480773926f / fAspectRatioScale)) < fTolerance;
}

void FOVFix()
{
	if (eGameType == Game::EFWW2IJ && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* UnderwaterValueCheckInstructionScanResult = Memory::PatternScan(dllModule2, "8A 46 61 84 C0 75 28 F6 46 44 08 74 22 8B 44 24 2C 8B CE");
		if (UnderwaterValueCheckInstructionScanResult)
		{
			spdlog::info("Underwater Value Check Instruction: Address is cshell.dll+{:x}", UnderwaterValueCheckInstructionScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid UnderwaterValueCheckMidHook{};

			UnderwaterValueCheckMidHook = safetyhook::create_mid(UnderwaterValueCheckInstructionScanResult, [](SafetyHookContext& ctx)
			{
				iIsUnderwater = *reinterpret_cast<uint16_t*>(ctx.esi + 0x61);
			});
		}
		else
		{
			spdlog::error("Failed to locate underwater value check instruction memory address.");
			return;
		}

		std::uint8_t* QuickLoadInstructionScanResult = Memory::PatternScan(dllModule2, "8A 0F 32 C0 84 C9 8B DF");
		if (QuickLoadInstructionScanResult)
		{
			spdlog::info("Quick Load Instruction: Address is cshell.dll+{:x}", QuickLoadInstructionScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid QuickLoadInstructionMidHook{};

			QuickLoadInstructionMidHook = safetyhook::create_mid(QuickLoadInstructionScanResult, [](SafetyHookContext& ctx)
			{
				const char* cMessage = reinterpret_cast<const char*>(ctx.edi);

				if (std::strcmp(cMessage, "QuickSave loaded.") == 0)
				{
					iIsUnderwater = 0;
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate quickload instruction memory address.");
			return;
		}

		iIsUnderwater = 0;

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B B0 50 01 00 00 89 B4 24 E0 00 00 00");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Access the HFOV value at the memory address EAX + 0x150
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax + 0x150);

				float& fCurrentCameraVFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x154);

				if (iIsUnderwater == 0)
				{
					if (fCurrentCameraHFOV == 0.7853981852531433f && fabsf(fCurrentCameraVFOV2 - (0.5890486240386963f / fAspectRatioScale)) < fTolerance)
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);
					}
					else if (bIsDefaultHFOV(fCurrentCameraHFOV) && bIsCroppedVFOV(fCurrentCameraVFOV2)) // Hipfire HFOV
					{
						fNewCameraHFOV = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV);
					}
					else
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);
					}
				}
				else if (iIsUnderwater == 1)
				{
					if (fCurrentCameraHFOV == 0.7853981852531433f && fabsf(fCurrentCameraVFOV2 - (0.5890486240386963f / fAspectRatioScale)) < fTolerance)
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);

						iIsUnderwater = 0;
					}
					else if (fCurrentCameraHFOV > 1.43f && fCurrentCameraHFOV < 1.71f)
					{
						fNewCameraHFOV = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV);
					}
					else
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);
					}
				}

				ctx.esi = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B B0 54 01 00 00 89 B4 24 E4 00 00 00");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x150);

				// Access the VFOV value at the memory address EAX + 0x154
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x154);								

				if (iIsUnderwater == 0)
				{
					if ((fCurrentCameraHFOV2 == 0.7853981852531433f || fCurrentCameraHFOV2 == CalculateNewHFOVWithoutFOVFactor(0.7853981852531433f)) && fabsf(fCurrentCameraVFOV - (0.5890486240386963f / fAspectRatioScale) < fTolerance))
					{
						fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV);
					}
					else if (bIsDefaultHFOV(fCurrentCameraHFOV2) && bIsCroppedVFOV(fCurrentCameraVFOV)) // Hipfire VFOV
					{
						fNewCameraVFOV = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);
					}
					else
					{
						fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);
					}
				}
				else if (iIsUnderwater == 1)
				{
					if (fCurrentCameraHFOV2 == 0.7853981852531433f && fabsf(fCurrentCameraVFOV - (0.5890486240386963f / fAspectRatioScale)) < fTolerance)
					{
						fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);

						iIsUnderwater = 0;
					}
					else if (fCurrentCameraVFOV > 1.0f / fAspectRatioScale && fCurrentCameraVFOV < 1.275f / fAspectRatioScale)
					{
						fNewCameraVFOV = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);
					}
					else
					{
						fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);
					}
				}

				ctx.esi = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction memory address.");
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