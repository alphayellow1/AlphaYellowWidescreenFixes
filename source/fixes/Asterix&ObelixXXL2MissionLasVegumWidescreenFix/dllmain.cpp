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
std::string sFixName = "Asterix&ObelixXXL2MissionLasVegumWidescreenFix";
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
float fNewCameraFOV;
float fNewAspectRatio2;

// Game detection
enum class Game
{
	AOXXL2,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Resolution1Scan,
	Resolution2Scan,
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AOXXL2, {"Asterix & Obelix XXL 2: Mission: Las Vegum", "GameModule.elb"}},
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

static SafetyHookMid ResolutionWidthInstruction1Hook{};
static SafetyHookMid ResolutionWidthInstruction2Hook{};
static SafetyHookMid ResolutionWidthInstruction3MidHook{};

static SafetyHookMid ResolutionHeightInstruction1MidHook{};
static SafetyHookMid ResolutionHeightInstruction2Hook{};
static SafetyHookMid ResolutionHeightInstruction3MidHook{};

static SafetyHookMid ResolutionInstructions4MidHook{};
static SafetyHookMid ResolutionInstructions5MidHook{};

static SafetyHookMid CameraAspectRatioInstructionMidHook{};

static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::AOXXL2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0A 89 0D 84 26 67 00 8B 6A 04", "8B 08 89 0D E0 27 67 00 8B 50 04", "74 17 8B 46 0C 8B 0D E0 27 67 00 3B C1 74 0A 0F AF C1 33 D2 F7 F7 89 46 0C 8B 3D 88 26 67 00 85 FF 74 17 8B 46 10 8B 0D E4 27 67 00 3B C1 74 0A 0F AF C1 33 D2 F7 F7 89 46 10", "89 56 0C 89 46 10 8B 0D 5C A5 66 00 89 4D 00", "89 95 28 07 00 00 89 06 89 0F 8B CD");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution5Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan], "\x90\x90", 2);
			
			ResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan] + 8, "\x90\x90\x90", 3);
			
			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan], "\x90\x90", 2);
			
			ResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan] + 8, "\x90\x90\x90", 3);
			
			ResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan] + 1, "\x14", 1);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan] + 14, "\x07", 1);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan] + 34, "\x14", 1);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan] + 47, "\x07", 1);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan] + 22, "\x90\x90\x90", 3);			

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution3Scan] + 22, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan] + 55, "\x90\x90\x90", 3);			

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution3Scan] + 55, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan], "\x90\x90\x90\x90\x90\x90", 6);			

			ResolutionInstructions4MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan], "\x90\x90\x90\x90\x90\x90\x90\x90", 8);
			
			ResolutionInstructions5MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ebp + 0x728) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi) = iCurrentResY;
			});
		}

		fNewAspectRatio2 = 0.75f / fAspectRatioScale;

		std::uint8_t* CameraAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 15 A4 A7 60 00 89 51 34 EB 3C 8B 41 04 85 C0 74 05");
		if (CameraAspectRatioInstructionScanResult)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraAspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraAspectRatioInstructionMidHook = safetyhook::create_mid(CameraAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(fNewAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* HUDAspectRatioScanResult = Memory::PatternScan(exeModule, "00 00 40 3F 00 00 00 3B E0 1A 44 00 A0 1A 44");
		if (HUDAspectRatioScanResult)
		{
			spdlog::info("HUD Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(HUDAspectRatioScanResult, fNewAspectRatio2);
		}
		else
		{
			spdlog::error("Failed to locate HUD aspect ratio memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 14 D8 0D B4 A7 60 00 8B 41 14 D8 0D 7C A0 60 00 D9 F2");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90", 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x14);

				fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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
