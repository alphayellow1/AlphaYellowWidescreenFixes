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
std::string sFixName = "StarTrekBridgeCommanderFOVFix";
std::string sFixVersion = "1.0.1";
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
int iCurrentResX;
int iCurrentResY;
bool bFixActive;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraHFOV2;
float fNewCameraVFOV;
float fNewCameraVFOV2;
float fNewExteriorHFOV1;
float fNewExteriorHFOV2;
float fNewExteriorVFOV;
uint8_t* ExteriorCameraFOVAddress;

// Game detection
enum class Game
{
	STBC,
	Unknown
};

enum CameraHFOVInstructionsIndex
{
	CameraHFOV1Scan,
	CameraHFOV2Scan,
	CameraHFOV3Scan,
	CameraHFOV4Scan,
	CameraHFOV5Scan,
	CameraHFOV6Scan,
	CameraHFOV7Scan,
	CameraHFOV8Scan,
	CameraHFOV9Scan,
	CameraHFOV10Scan,
	CameraHFOV11Scan,
	CameraHFOV12Scan,
	CameraHFOV13Scan,
	CameraHFOV14Scan
};

enum CameraVFOVInstructionsIndex
{
	CameraVFOV1Scan,
	CameraVFOV2Scan,
	CameraVFOV3Scan,
	CameraVFOV4Scan,
	CameraVFOV5Scan,
	CameraVFOV6Scan,
	CameraVFOV7Scan,
	CameraVFOV8Scan,
	CameraVFOV9Scan,
	CameraVFOV10Scan,
	CameraVFOV11Scan,
	CameraVFOV12Scan,
	CameraVFOV13Scan,
	CameraVFOV14Scan
};

enum ExteriorFOVInstructionsIndex
{
	ExteriorHFOV1Scan,
	ExteriorHFOV2Scan,
	ExteriorVFOVScan,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::STBC, {"Star Trek: Bridge Commander", "stbc.exe"}},
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

static SafetyHookMid CameraHFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraHFOVInstruction3Hook{};
static SafetyHookMid CameraHFOVInstruction4Hook{};
static SafetyHookMid CameraHFOVInstruction5Hook{};
static SafetyHookMid CameraHFOVInstruction6Hook{};
static SafetyHookMid CameraHFOVInstruction7Hook{};
static SafetyHookMid CameraHFOVInstruction8Hook{};
static SafetyHookMid CameraHFOVInstruction9Hook{};
static SafetyHookMid CameraHFOVInstruction10Hook{};
static SafetyHookMid CameraHFOVInstruction11Hook{};
static SafetyHookMid CameraHFOVInstruction12Hook{};
static SafetyHookMid CameraHFOVInstruction13Hook{};
static SafetyHookMid CameraHFOVInstruction14Hook{};

static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction3Hook{};
static SafetyHookMid CameraVFOVInstruction4Hook{};
static SafetyHookMid CameraVFOVInstruction5Hook{};
static SafetyHookMid CameraVFOVInstruction6Hook{};
static SafetyHookMid CameraVFOVInstruction7Hook{};
static SafetyHookMid CameraVFOVInstruction8Hook{};
static SafetyHookMid CameraVFOVInstruction9Hook{};
static SafetyHookMid CameraVFOVInstruction10Hook{};
static SafetyHookMid CameraVFOVInstruction11Hook{};
static SafetyHookMid CameraVFOVInstruction12Hook{};
static SafetyHookMid CameraVFOVInstruction13Hook{};
static SafetyHookMid CameraVFOVInstruction14Hook{};

static SafetyHookMid ExteriorHFOVInstruction1Hook{};
static SafetyHookMid ExteriorHFOVInstruction2Hook{};
static SafetyHookMid ExteriorVFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::STBC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> SpaceshipCameraHFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4", "D8 0D ?? ?? ?? ?? C7 84 24", "D8 0D ?? ?? ?? ?? 8B 8E", "D8 0D ?? ?? ?? ?? 8B 89");
		
		std::vector<std::uint8_t*> SpaceshipCameraVFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 83 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 4C 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4");
		
		if (Memory::AreAllSignaturesValid(SpaceshipCameraHFOVInstructionsScansResult) == true)
		{
			spdlog::info("Spaceship Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera HFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV6Scan] - (std::uint8_t*)exeModule);

			fNewCameraHFOV = 0.5f * fAspectRatioScale * fFOVFactor;

			fNewCameraHFOV2 = -0.5f * fAspectRatioScale * fFOVFactor;

			Memory::PatchBytes(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV1Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::PatchBytes(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::PatchBytes(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::PatchBytes(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction4Hook = safetyhook::create_mid(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV4Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV2);
			});

			Memory::PatchBytes(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction5Hook = safetyhook::create_mid(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV5Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV2);
			});

			Memory::PatchBytes(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction6Hook = safetyhook::create_mid(SpaceshipCameraHFOVInstructionsScansResult[CameraHFOV6Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV2);
			});
		}
		
		if (Memory::AreAllSignaturesValid(SpaceshipCameraVFOVInstructionsScansResult) == true)
		{
			spdlog::info("Spaceship Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Camera VFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV6Scan] - (std::uint8_t*)exeModule);

			fNewCameraVFOV = 0.375f * fFOVFactor;

			fNewCameraVFOV2 = -0.375f * fFOVFactor;

			Memory::PatchBytes(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction1Hook = safetyhook::create_mid(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV1Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV);
			});

			Memory::PatchBytes(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction2Hook = safetyhook::create_mid(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV);
			});

			Memory::PatchBytes(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction3Hook = safetyhook::create_mid(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV);
			});

			Memory::PatchBytes(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction4Hook = safetyhook::create_mid(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV4Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV2);
			});

			Memory::PatchBytes(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction5Hook = safetyhook::create_mid(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV5Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV2);
			});

			Memory::PatchBytes(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction6Hook = safetyhook::create_mid(SpaceshipCameraVFOVInstructionsScansResult[CameraVFOV6Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV2);
			});
		}

		std::vector<std::uint8_t*> ExteriorCameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "A1 ?? ?? ?? ?? 5E 89 44 24 ?? D8 7C 24", "D9 05 ?? ?? ?? ?? D9 E0 D9 5C 24 ?? D8 0D", "D8 0D ?? ?? ?? ?? D9 54 24 ?? D9 E0");
		if (Memory::AreAllSignaturesValid(ExteriorCameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Exterior Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ExteriorCameraFOVInstructionsScansResult[ExteriorHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Exterior Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ExteriorCameraFOVInstructionsScansResult[ExteriorHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Exterior Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), ExteriorCameraFOVInstructionsScansResult[ExteriorVFOVScan] - (std::uint8_t*)exeModule);

			ExteriorCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(ExteriorCameraFOVInstructionsScansResult[ExteriorHFOV1Scan] + 1, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ExteriorCameraFOVInstructionsScansResult[ExteriorHFOV1Scan], "\x90\x90\x90\x90\x90", 5);

			ExteriorHFOVInstruction1Hook = safetyhook::create_mid(ExteriorCameraFOVInstructionsScansResult[ExteriorHFOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentExteriorHFOV1 = *reinterpret_cast<float*>(ExteriorCameraFOVAddress);

				fNewExteriorHFOV1 = fCurrentExteriorHFOV1 * fAspectRatioScale * fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewExteriorHFOV1);
			});

			Memory::PatchBytes(ExteriorCameraFOVInstructionsScansResult[ExteriorHFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ExteriorHFOVInstruction2Hook = safetyhook::create_mid(ExteriorCameraFOVInstructionsScansResult[ExteriorHFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentExteriorHFOV2 = *reinterpret_cast<float*>(ExteriorCameraFOVAddress);

				fNewExteriorHFOV2 = fCurrentExteriorHFOV2 * fAspectRatioScale * fFOVFactor;

				FPU::FLD(fNewExteriorHFOV2);
			});

			Memory::PatchBytes(ExteriorCameraFOVInstructionsScansResult[ExteriorVFOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			ExteriorVFOVInstructionHook = safetyhook::create_mid(ExteriorCameraFOVInstructionsScansResult[ExteriorVFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentExteriorVFOV = *reinterpret_cast<float*>(ExteriorCameraFOVAddress);

				fNewExteriorVFOV = fCurrentExteriorVFOV * fAspectRatioScale * fFOVFactor;

				FPU::FMUL(fNewExteriorVFOV);
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