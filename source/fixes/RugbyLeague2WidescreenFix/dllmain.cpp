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
	Res5Scan
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

static SafetyHookMid ResolutionWidthInstructionHook{};
static SafetyHookMid ResolutionHeightInstructionHook{};
static SafetyHookMid Resolution5WidthInstructionHook{};
static SafetyHookMid Resolution5HeightInstructionHook{};
static SafetyHookMid HUDAspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructions15Hook{};
static SafetyHookMid CameraHFOVInstruction16Hook{};
static SafetyHookMid CameraVFOVInstruction16Hook{};

void WidescreenFix()
{
	if (eGameType == Game::RL2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "C2 00 C7 02 80 02 00 00 A1 94 10 C2 00 C7 40 04 E0 01 00 00", "C6 44 24 1C 05 C7 06 80 02 00 00 C7 46 04 E0 01 00 00", "68 E0 01 00 00 68 80 02 00 00 E8 C0 29 22 00 83 C4 0C C7 44 24 0C 38 E8 AE 00 89 5C 24 10 C7 44 24 14 E0 01 00 00 89 5C 24 18 C7 44 24 1C 80 02 00 00 8B 15 E0 AB BC 00 8B 42 24 8B 48 04 8B 01 8D 54 24 0C 52 89 5C 24 48 FF 50 50 A1 24 97 BC 00 8B 70 64 3B F3 74 3A 57 8B CE E8 8F 21 21 00 8B 78 60 8B CE E8 85 21 21 00 3B FB 8B 40 64 74 0E C7 47 0C 80 02 00 00 C7 47 10 E0 01 00 00 3B C3 5F 74 0E C7 40 0C 80 02 00 00 C7 40 10 E0 01 00", "8B 0F 89 0D 60 9B C2 00 8B 57 04 89 15 64 9B C2 00", "8B 81 4C 01 00 00 C3 90 90 90 90 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC A1 24 97 BC 00 8A 50 70 84 D2 74 06 B8 E0 01 00 00 C3 8B 49 10 8B 81 50 01 00 00 C3 90 90 90 90");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res5Scan] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[Res1Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res2Scan] + 7, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res2Scan] + 14, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 34, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 46, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 116, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 123, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 135, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 142, iCurrentResY);			

			ResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi) = iCurrentResX;
			});			

			ResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4Scan] + 8, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi + 0x4) = iCurrentResY;
			});			

			Resolution5WidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res5Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x14C) = iCurrentResX;
			});			

			Resolution5HeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res5Scan] + 48, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x150) = iCurrentResY;
			});
		}

		std::uint8_t* HUDAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 4F ?? 50 51 8B CE");
		if (HUDAspectRatioInstructionScanResult)
		{
			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(HUDAspectRatioInstructionScanResult, 3);
			
			HUDAspectRatioInstructionHook = safetyhook::create_mid(HUDAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDAspectRatio = *reinterpret_cast<float*>(ctx.edi + 0x34);

				if (fCurrentHUDAspectRatio != fNewHUDAspectRatio)
				{
					fNewHUDAspectRatio = fCurrentHUDAspectRatio * fAspectRatioScale;
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
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 5C 24", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 8B FF", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? CC", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 81 C4", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4C 24 ?? 5F",
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 90 90 90 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 8B C1",
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 90 90 90 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 6A",
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 CC", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C2 ?? ?? 90 90 90 90 90 90 CC", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E 83 C4 ?? C2 ?? ?? 90 90 90 90 90 90", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C2 ?? ?? 90 90 90 90 90 90 90", "8B 46 ?? 8B 4E ?? 50 51 8B 0D", "D9 46 ?? 83 EC ?? D8 66");
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

			Memory::WriteNOPs(CameraFOVInstructionScanResult[FOV16Scan] + 21, 3);

			CameraHFOVInstruction16Hook = safetyhook::create_mid(CameraFOVInstructionScanResult[FOV16Scan] + 21, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV16 = *reinterpret_cast<float*>(ctx.esi + 0x2C);

				if (fCurrentCameraHFOV16 != fNewCameraHFOV16)
				{
					fNewCameraHFOV16 = fCurrentCameraHFOV16 * fAspectRatioScale * fFOVFactor;
				}

				FPU::FLD(fNewCameraHFOV16);
			});

			Memory::WriteNOPs(CameraFOVInstructionScanResult[FOV16Scan], 3);

			CameraVFOVInstruction16Hook = safetyhook::create_mid(CameraFOVInstructionScanResult[FOV16Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV16 = *reinterpret_cast<float*>(ctx.esi + 0x30);

				if (fCurrentCameraVFOV16 != fNewCameraVFOV16)
				{
					fNewCameraVFOV16 = fCurrentCameraVFOV16 * fFOVFactor;
				}

				FPU::FLD(fNewCameraVFOV16);
			});
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