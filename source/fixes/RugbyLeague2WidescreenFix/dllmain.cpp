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
std::string sFixName = "RugbyLeague2WidescreenFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHUDAspectRatio;
float fNewCameraHFOV;
float fNewCameraHFOV15;
float fNewCameraHFOV16;
float fNewCameraVFOV;
float fNewCameraVFOV15;
float fNewCameraVFOV16;

// Game detection
enum class Game
{
	RL2,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1Scan,
	Res2Scan,
	Res3Scan,
	Res4Scan,
	Res5Scan,
	Res6Scan,
	Res7Scan,
	Res8Scan,
	Res9Scan,
	Res10Scan
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan,
	FOV4Scan,
	FOV5Scan,
	FOV6Scan,
	FOV7Scan,
	FOV8Scan,
	FOV9Scan,
	FOV10Scan,
	FOV11Scan,
	FOV12Scan,
	FOV13Scan,
	FOV14Scan,
	FOV15Scan,
	FOV16Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RL2, {"Rugby League 2", "RugbyLeague2.exe"}},
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

static SafetyHookMid ResolutionInstructions1Hook{};
static SafetyHookMid ResolutionInstructions2Hook{};
static SafetyHookMid ResolutionInstructions4Hook{};
static SafetyHookMid ResolutionWidthInstruction6Hook{};
static SafetyHookMid ResolutionHeightInstruction6Hook{};
static SafetyHookMid HUDAspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructions15Hook{};
static SafetyHookMid CameraHFOVInstruction16Hook{};
static SafetyHookMid CameraVFOVInstruction16Hook{};
static SafetyHookMid ViewportResolutionInstructions1Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction3Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction3Hook{};

void WidescreenFix()
{
	if (eGameType == Game::RL2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0F 89 0D", "89 28 8B 0D", "C7 47 ?? ?? ?? ?? ?? C7 47 ?? ?? ?? ?? ?? 3B C3", "89 73 ?? 89 7B ?? 89 70", "C7 40 ?? ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? C7 44 24", "8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C2 ?? ?? 90 90 90 90 90 90 90 90 90 8B 44 24", "C7 06 ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 8D 48", "C7 02 ?? ?? ?? ?? A1 ?? ?? ?? ?? C7 40", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C7 44 24", "C7 44 24 ?? ?? ?? ?? ?? 89 5C 24 ?? C7 44 24 ?? ?? ?? ?? ?? 8B 15");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res5Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res6Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res7Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res8Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res9Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Resolution Instructions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res10Scan] - (std::uint8_t*)exeModule);

			ResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edi + 0x4) = iCurrentResY;
			});

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 10, iCurrentResY);

			ResolutionInstructions4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4Scan], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::Write(ResolutionInstructionsScansResult[Res5Scan] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res5Scan] + 10, iCurrentResY);

			ResolutionWidthInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res6Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esp + 0x8) = iCurrentResX;
			});

			ResolutionHeightInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res6Scan] + 32, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esp + 0x8) = iCurrentResY;
			});

			Memory::Write(ResolutionInstructionsScansResult[Res7Scan] + 2, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res7Scan] + 9, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res7Scan] + 16, 32);
			
			Memory::Write(ResolutionInstructionsScansResult[Res8Scan] + 2, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res8Scan] + 14, iCurrentResY);
			
			Memory::Write(ResolutionInstructionsScansResult[Res9Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res9Scan] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res10Scan] + 16, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res10Scan] + 4, iCurrentResY);			
		}

		std::uint8_t* HUDAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 4F ?? 50 51 8B CE");
		if (HUDAspectRatioInstructionScanResult)
		{
			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(HUDAspectRatioInstructionScanResult, 3);
			
			HUDAspectRatioInstructionHook = safetyhook::create_mid(HUDAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDAspectRatio = *reinterpret_cast<float*>(ctx.edi + 0x34);

				if (fCurrentHUDAspectRatio == 315.979858398438f)
				{
					fNewHUDAspectRatio = fCurrentHUDAspectRatio * fAspectRatioScale;
				}
				else
				{
					fNewHUDAspectRatio = fCurrentHUDAspectRatio;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewHUDAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate HUD aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 53", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 5C 24", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 8B FF", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? CC", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 81 C4",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4C 24 ?? 5F", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 90 90 90 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 8B C1",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 90 90 90 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 6A",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 CC", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C2 ?? ?? 90 90 90 90 90 90 CC", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 90 90 90 90", 
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C2 ?? ?? 90 90 90 90 90 90 90", "8B 46 ?? 8B 4E ?? 50 51 8B 0D", "D8 46 ?? D9 5C 24 ?? D9 46 ?? D8 66");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionScanResult) == true)
		{
			spdlog::info("Camera FOV Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV9Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV10Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 11 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV11Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 12 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV12Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 13 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV13Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 14 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV14Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 15 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV15Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 16 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult[FOV16Scan] - (std::uint8_t*)exeModule);

			fNewCameraHFOV = 0.5f * fAspectRatioScale * fFOVFactor;

			fNewCameraVFOV = 0.375f * fFOVFactor;

			Memory::Write(CameraFOVInstructionScanResult[FOV1Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV1Scan] + 1, fNewCameraVFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV2Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV2Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV3Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV3Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV4Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV4Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV5Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV5Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV6Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV6Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV7Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV7Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV8Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV8Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV9Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV9Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV10Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV10Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV11Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV11Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV12Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV12Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV13Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV13Scan] + 1, fNewCameraVFOV);
			
			Memory::Write(CameraFOVInstructionScanResult[FOV14Scan] + 6, fNewCameraHFOV);

			Memory::Write(CameraFOVInstructionScanResult[FOV14Scan] + 1, fNewCameraVFOV);

			Memory::WriteNOPs(CameraFOVInstructionScanResult[FOV15Scan], 6);

			CameraFOVInstructions15Hook = safetyhook::create_mid(CameraFOVInstructionScanResult[FOV15Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV15 = *reinterpret_cast<float*>(ctx.esi + 0x54);
				
				if (fCurrentCameraHFOV15 != fNewCameraHFOV)
				{
					fNewCameraHFOV15 = fCurrentCameraHFOV15 * fAspectRatioScale * fFOVFactor;
				}
				
				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraHFOV15);
				
				float& fCurrentCameraVFOV15 = *reinterpret_cast<float*>(ctx.esi + 0x58);
				
				if (fCurrentCameraVFOV15 != fNewCameraVFOV15)
				{
					fNewCameraVFOV15 = fCurrentCameraVFOV15 * fFOVFactor;
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraVFOV15);
			});

			Memory::WriteNOPs(CameraFOVInstructionScanResult[FOV16Scan] + 16, 3);

			CameraHFOVInstruction16Hook = safetyhook::create_mid(CameraFOVInstructionScanResult[FOV16Scan] + 16, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV16 = *reinterpret_cast<float*>(ctx.esi + 0x38);

				if (fCurrentCameraHFOV16 != fNewCameraHFOV16)
				{
					fNewCameraHFOV16 = fCurrentCameraHFOV16 * fAspectRatioScale * fFOVFactor;
				}

				FPU::FADD(fNewCameraHFOV16);
			});

			Memory::WriteNOPs(CameraFOVInstructionScanResult[FOV16Scan], 3);

			CameraVFOVInstruction16Hook = safetyhook::create_mid(CameraFOVInstructionScanResult[FOV16Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV16 = *reinterpret_cast<float*>(ctx.esi + 0x3C);

				if (fCurrentCameraVFOV16 != fNewCameraVFOV16)
				{
					fNewCameraVFOV16 = fCurrentCameraVFOV16 * fFOVFactor;
				}

				FPU::FADD(fNewCameraVFOV16);
			});
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
			WidescreenFix();
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