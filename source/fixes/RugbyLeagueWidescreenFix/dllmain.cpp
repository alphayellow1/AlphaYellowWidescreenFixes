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
std::string sFixName = "RugbyLeagueWidescreenFix";
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

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewAspectRatio;
float fFOVFactor;
float fModifiedHFOVValue;
float fModifiedVFOVValue;

// Game detection
enum class Game
{
	RL,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RL, {"Rugby League", "RugbyLeague.exe"}},
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
	return fCurrentFOV * (fNewAspectRatio / fOldAspectRatio);
}

void WidescreenFix()
{
	if (eGameType == Game::RL && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::vector<const char*> resolutionPatterns =
		{
			// 640x480
			"6A 20 68 E0 01 00 00 68 80 02 00 00 C7 86 0C 01 00",
			"FF F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D",
			"E8 26 F9 F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D 44 24 20 81",
			"CF F1 F6 FF 68 E0 01 00 00 68 80 02 00 00 6A 00 50 8D 4C",
			"6A 20 BF E0 01 00 00 57 BE 80 02 00 00 56 E8 A8 85 0F",
			"C8 7D 49 BE 80 02 00 00 BF E0 01 00 00 51 8D 4C 24",
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

			spdlog::info("Resolution 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[3].address - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[4].address - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[5].address - (std::uint8_t*)exeModule);

			Memory::Write(resolutionPatternsResult[0].address + 8, iCurrentResX);

			Memory::Write(resolutionPatternsResult[0].address + 3, iCurrentResY);

			Memory::Write(resolutionPatternsResult[1].address + 9, iCurrentResX);

			Memory::Write(resolutionPatternsResult[1].address + 4, iCurrentResY);

			Memory::Write(resolutionPatternsResult[2].address + 11, iCurrentResX);

			Memory::Write(resolutionPatternsResult[2].address + 6, iCurrentResY);

			Memory::Write(resolutionPatternsResult[3].address + 10, iCurrentResX);

			Memory::Write(resolutionPatternsResult[3].address + 5, iCurrentResY);

			Memory::Write(resolutionPatternsResult[4].address + 9, iCurrentResX);

			Memory::Write(resolutionPatternsResult[4].address + 3, iCurrentResY);

			Memory::Write(resolutionPatternsResult[5].address + 4, iCurrentResX);

			Memory::Write(resolutionPatternsResult[5].address + 9, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution lists scan memory addresses.");
			return;
		}

		std::uint8_t* GameplayResolutionInstruction1ScanResult = Memory::PatternScan(exeModule, "89 0D C0 09 71 00 8B 57 04 89 15 C4 09 71 00 C7 05 D4 09 71 00 02 00 00 00");
		if (GameplayResolutionInstruction1ScanResult)
		{
			spdlog::info("Gameplay Resolution Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), GameplayResolutionInstruction1ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid GameplayResolutionWidthInstruction1MidHook{};

			GameplayResolutionWidthInstruction1MidHook = safetyhook::create_mid(GameplayResolutionInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uint32_t>(iCurrentResX);
			});

			static SafetyHookMid GameplayResolutionHeightInstruction1MidHook{};

			GameplayResolutionHeightInstruction1MidHook = safetyhook::create_mid(GameplayResolutionInstruction1ScanResult + 9, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uint32_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::info("Cannot locate the gameplay resolution instruction 1 memory address.");
			return;
		}

		std::uint8_t* GameplayResolutionInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 48 60 DB 41 0C 8B 51 10 89 54 24 14");
		if (GameplayResolutionInstruction2ScanResult)
		{
			spdlog::info("Gameplay Resolution Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), GameplayResolutionInstruction2ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid GameplayResolutionHeightInstruction2MidHook{};

			GameplayResolutionHeightInstruction2MidHook = safetyhook::create_mid(GameplayResolutionInstruction2ScanResult + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x10) = iCurrentResY;
			});

			static SafetyHookMid GameplayResolutionWidthInstruction2MidHook{};

			GameplayResolutionWidthInstruction2MidHook = safetyhook::create_mid(GameplayResolutionInstruction2ScanResult + 3, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::info("Cannot locate the gameplay resolution instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 4E 68 8B 50 04 D8 76 68 89 56 6C 8B 46 04 85 C0");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			static float fLastModifiedHFOV = 0.0f;

			static std::vector<float> vComputedHFOVs;

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Storing the current value of the ECX register in a float-type variable
				float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

				// Skip processing if this HFOV has already been computed
				if (std::find(vComputedHFOVs.begin(), vComputedHFOVs.end(), fCurrentCameraHFOV) != vComputedHFOVs.end())
				{
					return;
				}

				// Initializes the modified value to the current HFOV
				float fModifiedHFOVValue = fCurrentCameraHFOV;

				// Epsilon value for approximate float comparisons (tolerance)
				constexpr float epsilon = 1e-6f;

				// Helper lambda for approximate float comparisons
				auto isApproxEqual = [epsilon](float a, float b) -> bool
				{
					return std::fabs(a - b) < epsilon;
				};

				// Checks if the current HFOV matches any special value using approximate comparison or if it's below a threshold
				if (isApproxEqual(fCurrentCameraHFOV, 0.5f) ||
					isApproxEqual(fCurrentCameraHFOV, 0.5142856836f) ||
					isApproxEqual(fCurrentCameraHFOV, 0.5644456148f) ||
					isApproxEqual(fCurrentCameraHFOV, 0.846668303f) ||
					isApproxEqual(fCurrentCameraHFOV, 1.016010642f) ||
					isApproxEqual(fCurrentCameraHFOV, 0.3577670753f) ||
					fCurrentCameraHFOV < 0.1f)
				{
					fModifiedHFOVValue = CalculateNewFOV(fCurrentCameraHFOV);
				}

				// If the computed HFOV differs from the current one, updates the last modified value
				if (!isApproxEqual(fCurrentCameraHFOV, fModifiedHFOVValue))
				{
					fLastModifiedHFOV = fModifiedHFOVValue;
				}

				// Records the computed HFOV to avoid recalculating it in future calls
				vComputedHFOVs.push_back(fModifiedHFOVValue);

				// Updates the ECX register with the new HFOV value
				ctx.ecx = std::bit_cast<uintptr_t>(fModifiedHFOVValue);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction memory address.");
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