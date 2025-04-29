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
#include <algorithm>
#include <cmath>
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "PetzHorsez2WidescreenFix";
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

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fTolerance = 0.0001f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewHUDFOV;

// Game detection
enum class Game
{
	PH2CONFIG,
	PH2,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PH2CONFIG, {"Petz Horsez 2 - Configuration", "HorsezOption.exe"}},
	{Game::PH2, {"Petz Horsez 2", "Jade.exe"}},
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

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * atanf(tanf(fCurrentFOV / 2.0f) * (fNewAspectRatio / fOldAspectRatio));
}

void WidescreenFix()
{
	fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

	if (bFixActive == true)
	{
		if (eGameType == Game::PH2CONFIG)
		{
			std::vector<const char*> resolutionPatterns =
			{
				// 1680x1050
				"C4 90 06 00 00 75 24 81 7D B8 1A 04 00 00 75 1B 8B F4",
				"88 90 06 00 00 75 2F 81 BD 7C FF FF FF 1A 04 00 00 75 23 8B",
				"C4 90 06 00 00 0F 85 95 01 00 00 81 7D B8 1A 04 00 00 0F 85 88",
			};

			auto resolutionPatternsResult = Memory::MultiPatternScan(exeModule, resolutionPatterns);

			bool allFound = std::all_of(resolutionPatternsResult.begin(), resolutionPatternsResult.end(), [](const Memory::PatternInfo& info)
				{
					return info.address != nullptr;
				});

			if (allFound)
			{
				spdlog::info("Resolution 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[0].address - (std::uint8_t*)exeModule);

				spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[1].address - (std::uint8_t*)exeModule);

				spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[2].address - (std::uint8_t*)exeModule);

				Memory::Write(resolutionPatternsResult[0].address + 1, iCurrentResX);

				Memory::Write(resolutionPatternsResult[0].address + 10, iCurrentResY);

				Memory::Write(resolutionPatternsResult[1].address + 1, iCurrentResX);

				Memory::Write(resolutionPatternsResult[1].address + 13, iCurrentResY);

				Memory::Write(resolutionPatternsResult[2].address + 1, iCurrentResX);

				Memory::Write(resolutionPatternsResult[2].address + 14, iCurrentResY);
			}
			else
			{
				spdlog::info("Cannot locate the resolution lists scan memory addresses.");
				return;
			}
		}
		else if (eGameType == Game::PH2)
		{
			std::uint8_t* CutscenesCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 82 E0 55 CF 00 5D C3 CC CC");
			if (CutscenesCameraFOVInstructionScanResult)
			{
				spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CutscenesCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

				static SafetyHookMid CutscenesCameraFOVInstructionMidHook{};

				static std::vector<float> vComputedCutscenesFOVs;

				CutscenesCameraFOVInstructionMidHook = safetyhook::create_mid(CutscenesCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
					{
						float& fCurrentCutscenesCameraFOV = *reinterpret_cast<float*>(ctx.edx + 0xCF55E0);

						// Skip processing if a similar FOV (within tolerance) has already been computed
						bool alreadyComputed1 = std::any_of(vComputedCutscenesFOVs.begin(), vComputedCutscenesFOVs.end(),
							[&](float computedValue) {
								return std::fabs(computedValue - fCurrentCutscenesCameraFOV) < fTolerance;
							});
						if (alreadyComputed1)
						{
							return;
						}

						// Compute the new FOV value
						fCurrentCutscenesCameraFOV = CalculateNewFOV(fCurrentCutscenesCameraFOV);

						// Record the computed FOV for future calls
						vComputedCutscenesFOVs.push_back(fCurrentCutscenesCameraFOV);
					});
			}
			else
			{
				spdlog::info("Cannot locate the cutscenes camera FOV instruction memory address.");
				return;
			}

			std::uint8_t* GameplayCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 80 84 0C 00 00 D8 81 BC 0C 00 00 51 D9 1C 24");
			if (GameplayCameraFOVInstructionScanResult)
			{
				spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

				static SafetyHookMid GameplayCameraFOVInstructionMidHook{};

				static std::vector<float> vComputedGameplayFOVs;

				GameplayCameraFOVInstructionMidHook = safetyhook::create_mid(GameplayCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
					{
						float& fCurrentGameplayCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0xC84);

						// Skip processing if a similar FOV (within tolerance) has already been computed
						bool alreadyComputed2 = std::any_of(vComputedGameplayFOVs.begin(), vComputedGameplayFOVs.end(),
							[&](float computedValue) {
								return std::fabs(computedValue - fCurrentGameplayCameraFOV) < fTolerance;
							});
						if (alreadyComputed2)
						{
							return;
						}

						// Compute the new FOV value
						fCurrentGameplayCameraFOV = CalculateNewFOV(fCurrentGameplayCameraFOV) * fFOVFactor;

						// Record the computed FOV for future calls
						vComputedGameplayFOVs.push_back(fCurrentGameplayCameraFOV);
					});
			}
			else
			{
				spdlog::info("Cannot locate the gameplay camera FOV instruction memory address.");
				return;
			}

			std::uint8_t* HUDFOVScanResult = Memory::PatternScan(exeModule, "C7 45 DC 00 00 80 3F C7 45 F4 FF FF FF FF C7 45 F8 FE FE FE FE");
			if (HUDFOVScanResult)
			{
				spdlog::info("HUD FOV Scan: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVScanResult - (std::uint8_t*)exeModule);

				fNewHUDFOV = CalculateNewFOV(1.0f);
				
				Memory::Write(HUDFOVScanResult + 3, fNewHUDFOV);
			}
			else
			{
				spdlog::info("Cannot locate the HUD FOV scan memory address.");
				return;
			}
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