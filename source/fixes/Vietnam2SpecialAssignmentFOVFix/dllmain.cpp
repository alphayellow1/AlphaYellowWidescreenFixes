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
std::string sFixName = "Vietnam2SpecialAssignmentFOVFix";
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
constexpr float fTolerance = 0.000001f;
constexpr float fDefaultCameraHFOV = 1.5707963705062866f;
constexpr float fDefaultCameraVFOV = 1.3613568544387817f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraHFOV2;
float fNewCameraVFOV;
float fNewCameraVFOV2;
static float fUnderwaterCheckValue;

// Game detection
enum class Game
{
	V2SA,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::V2SA, {"Vietnam 2: Special Assignment", "BlackOps2.exe"}},
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
	return fabsf(fCurrentHFOV - fDefaultCameraHFOV) < fTolerance;
}

bool bIsDefaultVFOV(float fCurrentVFOV)
{
	return fabsf(fCurrentVFOV - fDefaultCameraVFOV) < fTolerance;
}

bool bIsCroppedVFOV(float fCurrentVFOV)
{
	return fabsf(fCurrentVFOV - (fDefaultCameraVFOV / fAspectRatioScale)) < fTolerance;
}

void FOVFix()
{
	if (eGameType == Game::V2SA && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;		

		std::uint8_t* UnderwaterCheckInstructionScanResult = Memory::PatternScan(exeModule, "8B 52 08 89 4C 24 6C 8B 0D ?? ?? ?? ?? 89 54 24 68 8B 50 04 89 4C 24 2C");
		if (UnderwaterCheckInstructionScanResult)
		{
			spdlog::info("Underwater Check Instruction: Address is {:s}+{:x}", sExeName.c_str(), UnderwaterCheckInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid UnderwaterCheckInstructionMidHook{};

			UnderwaterCheckInstructionMidHook = safetyhook::create_mid(UnderwaterCheckInstructionScanResult, [](SafetyHookContext& ctx)
			{
				fUnderwaterCheckValue = *reinterpret_cast<float*>(ctx.edx + 0x8);
			});
		}
		else
		{
			spdlog::error("Failed to locate underwater check instruction memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 88 98 01 00 00 89 94 24 E0 00 00 00");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax + 0x198);

				float& fCurrentCameraVFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x19C);

				if (fUnderwaterCheckValue == 1.0f) // Above water
				{
					if (bIsDefaultHFOV(fCurrentCameraHFOV) && (bIsDefaultVFOV(fCurrentCameraVFOV2) || bIsCroppedVFOV(fCurrentCameraVFOV2)))
					{
						fNewCameraHFOV = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV);
					}
					else
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);
					}
				}
				else if (fUnderwaterCheckValue == 0.7000000477f) // Under water
				{
					if (fCurrentCameraHFOV > 1.57f && fCurrentCameraHFOV < 1.59f)
					{
						fNewCameraHFOV = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV);
					}
					else
					{
						fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);
					}
				}
				else
				{
					fNewCameraHFOV = fCurrentCameraHFOV;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "8B B0 98 01 00 00 89 32");
		if (CameraHFOVInstruction2ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraHFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid CameraHFOVInstruction2MidHook{};
			
			CameraHFOVInstruction2MidHook = safetyhook::create_mid(CameraHFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV3 = *reinterpret_cast<float*>(ctx.eax + 0x198);

				float& fCurrentCameraVFOV3 = *reinterpret_cast<float*>(ctx.eax + 0x19C);

				if (fUnderwaterCheckValue == 1.0f) // Above water
				{
					if (bIsDefaultHFOV(fCurrentCameraHFOV3) && (bIsDefaultVFOV(fCurrentCameraVFOV3) || bIsCroppedVFOV(fCurrentCameraVFOV3)))
					{
						fNewCameraHFOV2 = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV3);
					}
					else
					{
						fNewCameraHFOV2 = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV3);
					}
				}
				else if (fUnderwaterCheckValue == 0.7000000477f) // Under water
				{
					if (fCurrentCameraHFOV3 > 1.57f && fCurrentCameraHFOV3 < 1.59f)
					{
						fNewCameraHFOV2 = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV3);
					}
					else
					{
						fNewCameraHFOV2 = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV3);
					}
				}
				else
				{
					fNewCameraHFOV2 = fCurrentCameraHFOV3;
				}

				ctx.esi = std::bit_cast<uintptr_t>(fNewCameraHFOV2);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 90 9C 01 00 00 3B CD 89 94 24 C8 00 00 00");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x19C);

				float& fCurrentCameraHFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x198);

				if (fUnderwaterCheckValue == 1.0f) // Above water
				{
					if (bIsDefaultHFOV(fCurrentCameraHFOV2) && (bIsDefaultVFOV(fCurrentCameraVFOV) || bIsCroppedVFOV(fCurrentCameraVFOV)))
					{
						fNewCameraVFOV = CalculateNewVFOVWithFOVFactor(fDefaultCameraVFOV);
					}
					else
					{
						fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV);
					}
				}
				else if (fUnderwaterCheckValue == 0.7000000477f) // Under water
				{
					if (fCurrentCameraVFOV > 1.34f && fCurrentCameraVFOV < 1.37f)
					{
						fNewCameraVFOV = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV);
					}
					else
					{
						fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV);
					}
				}
				else
				{
					fNewCameraVFOV = fCurrentCameraVFOV;
				}

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 80 9C 01 00 00 89 01");
		if (CameraVFOVInstruction2ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraVFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraVFOVInstruction2MidHook{};
			
			CameraVFOVInstruction2MidHook = safetyhook::create_mid(CameraVFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV4 = *reinterpret_cast<float*>(ctx.eax + 0x19C);

				float& fCurrentCameraHFOV4 = *reinterpret_cast<float*>(ctx.eax + 0x198);

				if (fUnderwaterCheckValue == 1.0f) // Above water
				{
					if (bIsDefaultHFOV(fCurrentCameraHFOV4) && (bIsDefaultVFOV(fCurrentCameraVFOV4) || bIsCroppedVFOV(fCurrentCameraVFOV4)))
					{
						fNewCameraVFOV2 = CalculateNewVFOVWithFOVFactor(fDefaultCameraVFOV);
					}
					else
					{
						fNewCameraVFOV2 = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV4);
					}
				}
				else if (fUnderwaterCheckValue == 0.7000000477f) // Under water
				{
					if (fCurrentCameraVFOV4 > 1.34f && fCurrentCameraVFOV4 < 1.37f)
					{
						fNewCameraVFOV2 = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV4);
					}
					else
					{
						fNewCameraVFOV2 = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV4);
					}
				}
				else
				{
					fNewCameraVFOV2 = fCurrentCameraVFOV4;
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraVFOV2);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction 2 memory address.");
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