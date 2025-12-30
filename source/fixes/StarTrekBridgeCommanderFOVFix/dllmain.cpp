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
std::string sFixVersion = "1.1";
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

enum SpaceshipInteriorHFOVInstructionsIndex
{
	InteriorHFOV1Scan,
	InteriorHFOV2Scan,
	InteriorHFOV3Scan,
	InteriorHFOV4Scan,
	InteriorHFOV5Scan,
	InteriorHFOV6Scan
};

enum SpaceshipInteriorVFOVInstructionsIndex
{
	InteriorVFOV1Scan,
	InteriorVFOV2Scan,
	InteriorVFOV3Scan,
	InteriorVFOV4Scan,
	InteriorVFOV5Scan,
	InteriorVFOV6Scan
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

static SafetyHookMid SpaceshipInteriorHFOVInstruction1Hook{};
static SafetyHookMid SpaceshipInteriorHFOVInstruction2Hook{};
static SafetyHookMid SpaceshipInteriorHFOVInstruction3Hook{};
static SafetyHookMid SpaceshipInteriorHFOVInstruction4Hook{};
static SafetyHookMid SpaceshipInteriorHFOVInstruction5Hook{};
static SafetyHookMid SpaceshipInteriorHFOVInstruction6Hook{};
static SafetyHookMid SpaceshipInteriorVFOVInstruction1Hook{};
static SafetyHookMid SpaceshipInteriorVFOVInstruction2Hook{};
static SafetyHookMid SpaceshipInteriorVFOVInstruction3Hook{};
static SafetyHookMid SpaceshipInteriorVFOVInstruction4Hook{};
static SafetyHookMid SpaceshipInteriorVFOVInstruction5Hook{};
static SafetyHookMid SpaceshipInteriorVFOVInstruction6Hook{};
static SafetyHookMid ExteriorHFOVInstruction1Hook{};
static SafetyHookMid ExteriorHFOVInstruction2Hook{};
static SafetyHookMid ExteriorVFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::STBC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> SpaceshipInteriorHFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4", "D8 0D ?? ?? ?? ?? C7 84 24", "D8 0D ?? ?? ?? ?? 8B 8E", "D8 0D ?? ?? ?? ?? 8B 89");
		
		std::vector<std::uint8_t*> SpaceshipInteriorVFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 83 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 4C 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4");
		
		if (Memory::AreAllSignaturesValid(SpaceshipInteriorHFOVInstructionsScansResult) == true)
		{
			spdlog::info("Spaceship Interior HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV6Scan] - (std::uint8_t*)exeModule);

			fNewCameraHFOV = 0.5f * fAspectRatioScale * fFOVFactor;

			fNewCameraHFOV2 = -0.5f * fAspectRatioScale * fFOVFactor;

			Memory::PatchBytes(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorHFOVInstruction1Hook = safetyhook::create_mid(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV1Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::PatchBytes(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorHFOVInstruction2Hook = safetyhook::create_mid(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::PatchBytes(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorHFOVInstruction3Hook = safetyhook::create_mid(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::PatchBytes(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorHFOVInstruction4Hook = safetyhook::create_mid(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV4Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV2);
			});

			Memory::PatchBytes(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorHFOVInstruction5Hook = safetyhook::create_mid(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV5Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV2);
			});

			Memory::PatchBytes(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorHFOVInstruction6Hook = safetyhook::create_mid(SpaceshipInteriorHFOVInstructionsScansResult[InteriorHFOV6Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraHFOV2);
			});
		}
		
		if (Memory::AreAllSignaturesValid(SpaceshipInteriorVFOVInstructionsScansResult) == true)
		{
			spdlog::info("Spaceship Interior VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV6Scan] - (std::uint8_t*)exeModule);

			fNewCameraVFOV = 0.375f * fFOVFactor;

			fNewCameraVFOV2 = -0.375f * fFOVFactor;

			Memory::PatchBytes(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorVFOVInstruction1Hook = safetyhook::create_mid(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV1Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV);
			});

			Memory::PatchBytes(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorVFOVInstruction2Hook = safetyhook::create_mid(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV);
			});

			Memory::PatchBytes(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorVFOVInstruction3Hook = safetyhook::create_mid(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV);
			});

			Memory::PatchBytes(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorVFOVInstruction4Hook = safetyhook::create_mid(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV4Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV2);
			});

			Memory::PatchBytes(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorVFOVInstruction5Hook = safetyhook::create_mid(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV5Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewCameraVFOV2);
			});

			Memory::PatchBytes(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			SpaceshipInteriorVFOVInstruction6Hook = safetyhook::create_mid(SpaceshipInteriorVFOVInstructionsScansResult[InteriorVFOV6Scan], [](SafetyHookContext& ctx)
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