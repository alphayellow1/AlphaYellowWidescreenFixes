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
HMODULE thisModule;

// Fix details
std::string sFixName = "DeepFighterFOVFix";
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
constexpr float fTolerance = 0.0001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fNewAspectRatio;
static float fCurrentCameraHFOV;

// Game detection
enum class Game
{
	DF,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DF, {"Deep Fighter", "df.exe"}},
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

float CalculateNewFOV(float fCurrentFOV)
{
	return fCurrentFOV * (fNewAspectRatio / fOldAspectRatio);
}

void FOVFix()
{
	if (eGameType == Game::DF && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		/*
		std::uint8_t* ResolutionWidthInstruction1ScanResult = Memory::PatternScan(exeModule, "8B 7B 0C 89 44 24 18 0F BF 53 1E");
		if (ResolutionWidthInstruction1ScanResult)
		{
			spdlog::info("Resolution Width Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstruction1ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionWidthInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ebx + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution width instruction 1 memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstruction1ScanResult = Memory::PatternScan(exeModule, "8B 43 10 03 C2 51 89 44 24 28");
		if (ResolutionHeightInstruction1ScanResult)
		{
			spdlog::info("Resolution Height Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstruction1ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionHeightInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ebx + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution height instruction 1 memory address.");
			return;
		}

		std::uint8_t* ResolutionWidthInstruction2and3ScanResult = Memory::PatternScan(exeModule, "8B 50 0C 8B 79 0C 3B D7 5F 75 0A");
		if (ResolutionWidthInstruction2and3ScanResult)
		{
			spdlog::info("Resolution Width Instruction 2 and 3: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstruction2and3ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstruction2and3MidHook{};

			ResolutionWidthInstruction2and3MidHook = safetyhook::create_mid(ResolutionWidthInstruction2and3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0xC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.ecx + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution width instruction 2 and 3 memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstruction2and3ScanResult = Memory::PatternScan(exeModule, "8B 40 10 8B 51 10 3B C2 74 27 6A 17 C7 44 24 08 01 00 00 00");
		if (ResolutionHeightInstruction2and3ScanResult)
		{
			spdlog::info("Resolution Height Instruction 2 and 3: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstruction2and3ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstruction2and3MidHook{};

			ResolutionHeightInstruction2and3MidHook = safetyhook::create_mid(ResolutionHeightInstruction2and3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0x10) = iCurrentResY;

				*reinterpret_cast<int*>(ctx.ecx + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution height instruction 2 and 3 memory address.");
			return;
		}

		std::uint8_t* ResolutionWidthInstruction4ScanResult = Memory::PatternScan(exeModule, "8B 40 0C C3 90 90 90 90 90 90 90 90");
		if (ResolutionWidthInstruction4ScanResult)
		{
			spdlog::info("Resolution Width Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstruction4ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstruction4MidHook{};

			ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionWidthInstruction4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution width instruction 4 memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstruction4ScanResult = Memory::PatternScan(exeModule, "8B 40 10 C3 90 90 90 90 90 90 90 90");
		if (ResolutionHeightInstruction4ScanResult)
		{
			spdlog::info("Resolution Height Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstruction4ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstruction4MidHook{};

			ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionHeightInstruction4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution height instruction 4 memory address.");
			return;
		}

		std::uint8_t* ResolutionWidthInstruction5ScanResult = Memory::PatternScan(exeModule, "8B 48 0C 89 0D 40 62 67 00");
		if (ResolutionWidthInstruction5ScanResult)
		{
			spdlog::info("Resolution Width Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstruction5ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstruction5MidHook{};

			ResolutionWidthInstruction5MidHook = safetyhook::create_mid(ResolutionWidthInstruction5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution width instruction 5 memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstruction5ScanResult = Memory::PatternScan(exeModule, "8B 50 10 89 15 44 62 67 00 B8 01 00 00 00");
		if (ResolutionHeightInstruction5ScanResult)
		{
			spdlog::info("Resolution Height Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstruction5ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstruction5MidHook{};

			ResolutionHeightInstruction5MidHook = safetyhook::create_mid(ResolutionHeightInstruction5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution height instruction 5 memory address.");
			return;
		}

		std::uint8_t* ResolutionWidthInstruction6ScanResult = Memory::PatternScan(exeModule, "8B 50 0C 89 54 24 08");
		if (ResolutionWidthInstruction6ScanResult)
		{
			spdlog::info("Resolution Width Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstruction6ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstruction6MidHook{};

			ResolutionWidthInstruction6MidHook = safetyhook::create_mid(ResolutionWidthInstruction6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution width instruction 6 memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstruction6ScanResult = Memory::PatternScan(exeModule, "8B 40 10 8B 54 24 1C 89 44 24 0C");
		if (ResolutionHeightInstruction6ScanResult)
		{
			spdlog::info("Resolution Height Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstruction6ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstruction6MidHook{};

			ResolutionHeightInstruction6MidHook = safetyhook::create_mid(ResolutionHeightInstruction6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution height instruction 6 memory address.");
			return;
		}
		*/

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 4E 60 8B 50 04");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			static std::vector<float> vComputedHFOVs;

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Convert the ECX register value to float
				fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

				// Skip processing if a similar HFOV (within tolerance) has already been computed
				bool bHFOVAlreadyComputed = std::any_of(vComputedHFOVs.begin(), vComputedHFOVs.end(),
					[&](float computedValue) {
					return std::fabs(computedValue - fCurrentCameraHFOV) < fTolerance;
				});

				if (bHFOVAlreadyComputed)
				{
					return;
				}

				fCurrentCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV) * fFOVFactor;

				// Record the computed HFOV for future calls
				vComputedHFOVs.push_back(fCurrentCameraHFOV);

				// Update the ECX register with the new HFOV value
				ctx.ecx = std::bit_cast<uintptr_t>(fCurrentCameraHFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 56 64 8B 46 04 D9 46 60");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			static std::vector<float> vComputedVFOVs;

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Convert the EDX register value to float
				float fCurrentCameraVFOV = std::bit_cast<float>(ctx.edx);

				// Skip processing if a similar VFOV (within tolerance) has already been computed
				bool bVFOVAlreadyComputed = std::any_of(vComputedVFOVs.begin(), vComputedVFOVs.end(),
					[&](float computedValue) {
					return std::fabs(computedValue - fCurrentCameraVFOV) < fTolerance;
				});

				if (bVFOVAlreadyComputed)
				{
					return;
				}

				// Compute the new VFOV value
				fCurrentCameraVFOV *= fFOVFactor;

				// Record the computed VFOV for future calls
				vComputedVFOVs.push_back(fCurrentCameraVFOV);

				// Update the EDX register with the new VFOV value
				ctx.edx = std::bit_cast<uintptr_t>(fCurrentCameraVFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera VFOV instruction memory address.");
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