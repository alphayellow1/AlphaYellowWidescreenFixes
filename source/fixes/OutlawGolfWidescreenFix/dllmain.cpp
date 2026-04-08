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
std::string sFixName = "OutlawGolfWidescreenFix";
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

// Ini variables
bool bFixActive;
uint32_t iNewWindowedWidth;
uint32_t iNewWindowedHeight;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalCameraFOV = 50.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHUDAspectRatio;
float fNewCameraFOV;

// Game detection
enum class Game
{
	OG,
	Unknown
};

enum ResolutionInstructionsIndex
{
	FullscreenResListUnlock,
	ResWidthHeight,
	WindowedRes1,
	WindowedRes2,
	WindowedRes3,
	WindowedRes4
};

enum AspectRatioInstructionsIndex
{
	CameraAR1,
	CameraAR2,
	HUDAR
};

enum CameraFOVInstructionsIndex
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	FOV5,
	FOV6,
	FOV7,
	FOV8,
	FOV9,
	FOV10
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::OG, {"Outlaw Golf", "OLG1PC.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "WindowedWidth", iNewWindowedWidth);
	inipp::get_value(ini.sections["Settings"], "WindowedHeight", iNewWindowedHeight);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iNewWindowedWidth);
	spdlog_confparse(iNewWindowedHeight);
	spdlog_confparse(fFOVFactor);

	// If resolution not specified, use desktop resolution
	if (iNewWindowedWidth <= 0 || iNewWindowedHeight <= 0)
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();
		iNewWindowedWidth = desktopDimensions.first;
		iNewWindowedHeight = desktopDimensions.second;
		spdlog_confparse(iNewWindowedWidth);
		spdlog_confparse(iNewWindowedHeight);
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

static SafetyHookMid ResolutionInstructionsHook{};
static SafetyHookMid CameraAspectRatioInstruction1Hook{};
static SafetyHookMid CameraAspectRatioInstruction2Hook{};
static SafetyHookMid HUDAspectRatioInstructionHook{};

void SetARAndFOV()
{
	std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 70 ?? D9 5C 24", "D8 76 ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 83 C4",
	"D9 40 ?? DC C0 D9 1D ?? ?? ?? ?? D9 40 ?? DC C0 D9 1D ?? ?? ?? ?? 8B 48 ?? 81 F9 ?? ?? ?? ?? 74 ?? 39 05 ?? ?? ?? ?? 74 ?? 89 0D ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? A3 ?? ?? ?? ?? 89 15");
	if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
	{
		spdlog::info("Camera Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CameraAR1] - (std::uint8_t*)exeModule);

		spdlog::info("Camera Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CameraAR2] - (std::uint8_t*)exeModule);

		spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUDAR] - (std::uint8_t*)exeModule);

		Memory::WriteNOPs(AspectRatioInstructionsScansResult[CameraAR1], 3);

		CameraAspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CameraAR1], [](SafetyHookContext& ctx)
		{
			FPU::FDIV(fNewAspectRatio);
		});

		Memory::WriteNOPs(AspectRatioInstructionsScansResult[CameraAR2], 3);

		CameraAspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CameraAR2], [](SafetyHookContext& ctx)
		{
			FPU::FDIV(fNewAspectRatio);
		});

		Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUDAR], 3);

		HUDAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUDAR], [](SafetyHookContext& ctx)
		{
			float& fCurrentHUDAspectRatio = Memory::ReadMem(ctx.eax + 0x68);

			fNewHUDAspectRatio = fCurrentHUDAspectRatio / fAspectRatioScale;

			FPU::FLD(fNewHUDAspectRatio);
		});
	}

	std::vector<std::uint8_t*> CameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50",
	"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 56",
	"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 35 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 35 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1", "68 ?? ?? ?? ?? F3 ?? 68 ?? ?? ?? ?? 89 2D",
	"68 ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? 83 C4 ?? C6 05", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 74 24",
	"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 68");
	if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScanResult) == true)
	{
		spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV1] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV2] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV3] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV4] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV5] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV6] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV7] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV8] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV9] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV10] - (std::uint8_t*)exeModule);

		fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fOriginalCameraFOV, fAspectRatioScale);

		Memory::Write(CameraFOVInstructionsScanResult, FOV1, FOV9, 1, fNewCameraFOV * fFOVFactor);

		Memory::Write(CameraFOVInstructionsScanResult[FOV10] + 1, fNewCameraFOV);
	}
}

void FOVFix()
{
	if (eGameType == Game::OG && bFixActive == true)
	{
		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "74 ?? 81 F9 ?? ?? ?? ?? 7C", "8B 02 A3 ?? ?? ?? ?? 8B 42",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 33 C0", 
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F0", "C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 5F 5E 5B", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 44 24");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Fullscreen Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[FullscreenResListUnlock] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResWidthHeight] - (std::uint8_t*)exeModule);

			spdlog::info("Windowed Resolution Instructions 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[WindowedRes1] - (std::uint8_t*)exeModule);

			spdlog::info("Windowed Resolution Instructions 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[WindowedRes2] - (std::uint8_t*)exeModule);

			spdlog::info("Windowed Resolution Instructions 3: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[WindowedRes3] - (std::uint8_t*)exeModule);

			spdlog::info("Windowed Resolution Instructions 4: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[WindowedRes4] - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionInstructionsScansResult[FullscreenResListUnlock], "\xEB");

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);

				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				SetARAndFOV();

				ResolutionInstructionsHook.reset();
			});

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes1] + 6, iNewWindowedWidth);

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes1] + 16, iNewWindowedHeight);

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes2] + 6, iNewWindowedWidth);

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes2] + 1, iNewWindowedHeight);

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes3] + 10, iNewWindowedWidth);

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes3] + 3, iNewWindowedHeight);

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes4] + 6, iNewWindowedWidth);

			Memory::Write(ResolutionInstructionsScansResult[WindowedRes4] + 1, iNewWindowedHeight);
		}		
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		thisModule = hModule;
		Logging();
		Configuration();
		if (DetectGame())
		{
			FOVFix();
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