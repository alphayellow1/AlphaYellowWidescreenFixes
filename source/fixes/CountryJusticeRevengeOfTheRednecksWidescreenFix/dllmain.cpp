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
std::string sFixName = "CountryJusticeRevengeOfTheRednecksWidescreenFix";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;
uint8_t* CameraVFOVAddress;
int iNewViewportWidth4;
int iNewViewportHeight4;
int iNewViewportWidth5;
int iNewViewportHeight5;
static int* NewResXPtr;
static int* NewResYPtr;

// Game detection
enum class Game
{
	CJROTR,
	Unknown
};

enum ResolutionInstructionsIndex
{
	RendererScan,
	Viewport1Scan,
	Viewport2Scan,
	Viewport3Scan,
	Viewport4Scan,
	Viewport5Scan,
	Viewport6Scan,
	Viewport7Scan,
	Viewport8Scan,
	Viewport9Scan,
	Viewport10Scan,
	Viewport11Scan,
	Viewport12Scan,
	Viewport13Scan,
	Viewport14Scan,
	Viewport15Scan,
	Viewport16Scan,
	Viewport17Scan,
	Viewport18Scan,
	Viewport19Scan,
	Viewport20Scan,
	Viewport21Scan,
	Viewport22Scan,
	Viewport23Scan,
	Viewport24Scan,
	Viewport25Scan,
	Viewport26Scan,
	Viewport27Scan,
	Viewport28Scan,
	Viewport29Scan,
	Viewport30Scan,
	Viewport31Scan,
	Viewport32Scan,
	Viewport33Scan,
	Viewport34Scan,
	Viewport35Scan,
	Viewport36Scan,
	Viewport37Scan,
	Viewport38Scan,
	Viewport39Scan,
	Viewport40Scan,
	Viewport41Scan,
	Viewport42Scan,
	Viewport43Scan,
	Viewport44Scan,
	Viewport45Scan,
	Viewport46Scan,
	Viewport47Scan,
	Viewport48Scan,
	Viewport49Scan,
	Viewport50Scan,
	Viewport51Scan,
	Viewport52Scan,
	Viewport53Scan,
	Viewport54Scan,
	Viewport55Scan,
	Viewport56Scan,
	Viewport57Scan,
	Viewport58Scan,
	Viewport59Scan,
	Viewport60Scan,
	Viewport61Scan,
	Viewport62Scan,
	Viewport63Scan,
	Viewport64Scan,
	Viewport65Scan,
	Viewport66Scan,
	Viewport67Scan,
	Viewport68Scan,
	Viewport69Scan,
	Viewport70Scan,
	Viewport71Scan,
	Viewport72Scan,
	Viewport73Scan,
	Viewport74Scan,
	Viewport75Scan,
	Viewport76Scan,
	Viewport77Scan,
	Viewport78Scan,
	Viewport79Scan,
	Viewport80Scan,
	Viewport81Scan,
	Viewport82Scan,
	Viewport83Scan,
	Viewport84Scan,
	Viewport85Scan,
	Viewport86Scan,
	Viewport87Scan,
	Viewport88Scan,
	Viewport89Scan,
	Viewport90Scan,
	Viewport91Scan,
	Viewport92Scan,
	Viewport93Scan,
	Viewport94Scan,
	Viewport95Scan,
	Viewport96Scan,
	Viewport97Scan,
	Viewport98Scan
};

enum CameraFOVInstructionsIndex
{
	HFOVScan,
	VFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CJROTR, {"Country Justice: Revenge of the Rednecks", "Country Justice.exe"}},
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

static SafetyHookMid RendererResolutionWidthInstructionHook{};
static SafetyHookMid RendererResolutionHeightInstructionHook{};
static SafetyHookMid ViewportResolutionInstructions1Hook{};
static SafetyHookMid ViewportResolutionInstructions2Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction3Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction3Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction4Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction4Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction5Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction5Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction6Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction6Hook{};
static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::CJROTR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8b 08 89 0d ? ? ? ? 8b 50", "8b 44 24 ? 8b 4c 24 ? a3 ? ? ? ? 89 0d ? ? ? ? a1 ? ? ? ? 68 ? ? ? ? 50", "8b 54 24 ? 8b 44 24 ? 89 15", "8b 57 ? 89 54 24 ? 8b 47", "8b 46 ? 8b 0d ? ? ? ? 3b c1 74 ? 0f af ? 33 d2 f7 f7 89 46 ? 8b 3d", "8b 50 ? 89 54 24 ? 8b 40 ? 89 44 24 ? a1 ? ? ? ? 8d 54 24", "8b 55 ? 89 44 24 ? 8b 45 ? 89 54 24 ? 89 44 24 ? a1 ? ? ? ? 8d 54 24 ? 89 4c 24 ? c7 44 24 ? ? ? ? ? c7 44 24 ? ? ? ? ? 8b 08 52 50 ff 91 ? ? ? ? 8b 44 24",
		"8b 90 ? ? ? ? 8b 46 ? c1 f9 ? c1 fa ? 51 8b 8e ? ? ? ? 52 50 e8 ? ? ? ? 85 c0", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 0d ? ? ? ? 8b 81 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? a1 ? ? ? ? 3b c7", "8b 88 ? ? ? ? 89 8a ? ? ? ? a1", "8b 90 ? ? ? ? 51 52 8b ce", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 15 ? ? ? ? 8b 82 ? ? ? ? 89 8e ? ? ? ? 99 2b c2 89 8e ? ? ? ? d1 f8 83 e8 ? 89 8e ? ? ? ? 89 86 ? ? ? ? b8 ? ? ? ? 89 8e ? ? ? ? 89 8e ? ? ? ? 89 8e", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 15 ? ? ? ? 8b 82 ? ? ? ? 89 8e ? ? ? ? 99 2b c2 89 8e ? ? ? ? d1 f8 83 e8 ? 89 8e ? ? ? ? 89 86 ? ? ? ? b8 ? ? ? ? 89 8e ? ? ? ? 2b f8", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 15 ? ? ? ? 8b 82 ? ? ? ? 89 8e ? ? ? ? 99 2b c2 89 8e ? ? ? ? d1 f8 83 e8 ? 89 8e ? ? ? ? 89 86 ? ? ? ? b8 ? ? ? ? 89 8e ? ? ? ? 89 8e ? ? ? ? c7 86", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 0d ? ? ? ? 8b 81 ? ? ? ? 89 9e", "8b 90 ? ? ? ? 8b 46 ? c1 f9 ? c1 fa ? 51 8b 8e ? ? ? ? 52 50 e8 ? ? ? ? 8b 86", "8b 80 ? ? ? ? 6a ? 6a ? 83 ea", "8b 90 ? ? ? ? 6a", "8b 82 ? ? ? ? d1 f8 89 44 24", "8b 91 ? ? ? ? b9 ? ? ? ? d1 fa 89 54 24 ? db 44 24", "8b 91 ? ? ? ? b9 ? ? ? ? d1 fa 89 54 24 ? c7 86", "8b 88 ? ? ? ? d1 f9 89 4c 24 ? 51", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 0d ? ? ? ? 8b 81 ? ? ? ? 99 2b c2 8d 96 ? ? ? ? d1 f8 83 e8 ? 89 86 ? ? ? ? b8 ? ? ? ? 2b d0 8a 08 88 0c ? 40 3a cb 75 ? 68 ? ? ? ? 8b ce e8 ? ? ? ? 8b 4c 24 ? 89 9e ? ? ? ? 89 9e ? ? ? ? 8b c6 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c2 ? ? 90 90 90 90 90 90 90 90 90 56 8b f1 e8 ? ? ? ? f6 44 24 ? ? 74 ? 56 e8 ? ? ? ? 83 c4 ? 8b c6 5e c2 ? ? 90 90 6a ? 68 ? ? ? ? 64 a1 ? ? ? ? 50 64 89 25 ? ? ? ? 51 53 56 8b f1 57 89 74 24 ? c7 06 ? ? ? ? 8b 86 ? ? ? ? 8d be ? ? ? ? 8b cf c7 44 24 ? ? ? ? ? ff 50 ? 8b 56 ? 8d 5e ? 8b cb ff 52 ? 8b cf c7 44 24 ? ? ? ? ? e8 ? ? ? ? 8b cb c6 44 24 ? ? e8 ? ? ? ? 8b 4c 24 ? c7 06 ? ? ? ? 5f 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c3 90 90 90 90 64 a1 ? ? ? ? 6a ? 68 ? ? ? ? 50 8b 44 24 ? 64 89 25 ? ? ? ? 81 ec ? ? ? ? 55 56 8b f1 33 ed 3b c5 89 86 ? ? ? ? 89 ae ? ? ? ? 89 ae ? ? ? ? 75 ? 8b 46 ? 8d 8e ? ? ? ? 24 ? 89 46 ? 8b 01 ff 50 ? e9 ? ? ? ? 8b 0d", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 0d ? ? ? ? 8b 81 ? ? ? ? 99 2b c2 8b d6", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 0d ? ? ? ? 8b 81 ? ? ? ? 99 2b c2 8d 96 ? ? ? ? d1 f8 83 e8 ? 89 86 ? ? ? ? b8 ? ? ? ? 2b d0 8a 08 88 0c ? 40 3a cb 75 ? 68 ? ? ? ? 8b ce e8 ? ? ? ? 8b 4c 24 ? 89 9e ? ? ? ? 89 9e ? ? ? ? 8b c6 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c2 ? ? 90 90 90 90 90 90 90 90 90 56 8b f1 e8 ? ? ? ? f6 44 24 ? ? 74 ? 56 e8 ? ? ? ? 83 c4 ? 8b c6 5e c2 ? ? 90 90 6a ? 68 ? ? ? ? 64 a1 ? ? ? ? 50 64 89 25 ? ? ? ? 51 53 56 8b f1 57 89 74 24 ? c7 06 ? ? ? ? 8b 86 ? ? ? ? 8d be ? ? ? ? 8b cf c7 44 24 ? ? ? ? ? ff 50 ? 8b 56 ? 8d 5e ? 8b cb ff 52 ? 8b cf c7 44 24 ? ? ? ? ? e8 ? ? ? ? 8b cb c6 44 24 ? ? e8 ? ? ? ? 8b 4c 24 ? c7 06 ? ? ? ? 5f 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c3 90 90 90 90 64 a1 ? ? ? ? 6a ? 68 ? ? ? ? 50 8b 44 24 ? 64 89 25 ? ? ? ? 81 ec ? ? ? ? 55 56 8b f1 33 ed 3b c5 89 86 ? ? ? ? 89 ae ? ? ? ? 89 ae ? ? ? ? 75 ? 8b 46 ? 8d 8e ? ? ? ? 24 ? 89 46 ? 8b 01 ff 50 ? e9 ? ? ? ? 8b 8c 24", "8b 80 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? 8b 0d ? ? ? ? 8b 81 ? ? ? ? 99 2b c2 8d 96 ? ? ? ? d1 f8 83 e8 ? 89 86 ? ? ? ? b8 ? ? ? ? 2b d0 8a 08 88 0c ? 40 3a cb 75 ? 68 ? ? ? ? 8b ce e8 ? ? ? ? 8b 4c 24 ? 8b c6", "8b 87 ? ? ? ? 83 e8 ? 8b ce 50 e8 ? ? ? ? 39 ae ? ? ? ? 0f 84", "8b 97 ? ? ? ? d1 fa 89 54 24", "8b 90 ? ? ? ? d1 fa", "8b 88 ? ? ? ? d1 f9 89 4c 24 ? db 44 24", "8b 8d ? ? ? ? 8b 15 ? ? ? ? d1 f9 2b c8 51", "8b 88 ? ? ? ? d1 f9 89 8e", "8b 82 ? ? ? ? 53", "8b 82 ? ? ? ? c7 86", "8b 8f ? ? ? ? d1 f9 2b c8 89 8e ? ? ? ? 8b 3d ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? e8 ? ? ? ? 8b 97 ? ? ? ? d1 fa 2b d0 8b 84 24", "8b 91 ? ? ? ? d1 fa 2b d0", "8b 97 ? ? ? ? d1 fa 2b d0 89 96", "8b 8f ? ? ? ? 50 83 e9 ? 51 eb", "8b 8f ? ? ? ? 50 83 e9 ? 51 8b ce",
		"db 86 ? ? ? ? 51", "db 86 ? ? ? ? d8 0d ? ? ? ? d9 96 ? ? ? ? db 86", "db 80 ? ? ? ? 8b 46 ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 80 ? ? ? ? 50 e8 ? ? ? ? 83 c4 ? 8b 56", "db 80 ? ? ? ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 82 ? ? ? ? 50 e8 ? ? ? ? 8b 0d", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? 89 44 24 ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 d9 c9 d8 8f ? ? ? ? de c1 e8 ? ? ? ? 89 44 24 ? db 43 ? d8 8f ? ? ? ? e8 ? ? ? ? db 44 24 ? 89 44 24 ? d8 8f ? ? ? ? e8 ? ? ? ? 8b 8e ? ? ? ? 89 44 24 ? 8b 43 ? 8d 54 24 ? 52 50 51 e8 ? ? ? ? 8b 86 ? ? ? ? 8d 54 24 ? 52 50 e8 ? ? ? ? 83 c4 ? a1 ? ? ? ? 3b c5 7e ? 50 8d 8c 24 ? ? ? ? 68 ? ? ? ? 51 e8 ? ? ? ? 8b 3d ? ? ? ? 83 c4 ? 8d 94 24 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 55 6a ? 6a ? 55 de e9 6a ? 52 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? d9 05", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? d9 05", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? d9 05", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? 89 44 24 ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 d9 c9 d8 8f ? ? ? ? de c1 e8 ? ? ? ? 89 44 24 ? db 43 ? d8 8f ? ? ? ? e8 ? ? ? ? db 44 24 ? 89 44 24 ? d8 8f ? ? ? ? e8 ? ? ? ? 8b 8e ? ? ? ? 89 44 24 ? 8b 43 ? 8d 54 24 ? 52 50 51 e8 ? ? ? ? 8b 86 ? ? ? ? 8d 54 24 ? 52 50 e8 ? ? ? ? 83 c4 ? a1 ? ? ? ? 3b c5 7e ? 50 8d 8c 24 ? ? ? ? 68 ? ? ? ? 51 e8 ? ? ? ? 8b 3d ? ? ? ? 83 c4 ? 8d 94 24 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 55 6a ? 6a ? 55 de e9 6a ? 52 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 8b 8e", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 8b 8e", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 8b 8e", "db 80 ? ? ? ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 82 ? ? ? ? 50 e8 ? ? ? ? 83 c4 ? eb ? 55", "db 80 ? ? ? ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 82 ? ? ? ? 50 e8 ? ? ? ? 83 c4 ? eb ? 8b 8e", "da b0", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? 89 44 24 ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 d9 c9 d8 8f ? ? ? ? de c1 e8 ? ? ? ? 89 44 24 ? db 43 ? d8 8f ? ? ? ? e8 ? ? ? ? db 44 24 ? 89 44 24 ? d8 8f ? ? ? ? e8 ? ? ? ? 89 44 24", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 83 c4",
		"8b 88 ? ? ? ? 8b 90 ? ? ? ? 8b 46 ? c1 f9 ? c1 fa ? 51 8b 8e ? ? ? ? 52 50 e8 ? ? ? ? 85 c0", "8b 81 ? ? ? ? 99 2b c2 d1 f8 2d ? ? ? ? 89 86 ? ? ? ? a1 ? ? ? ? 3b c7", "8b 88 ? ? ? ? 89 8a ? ? ? ? 89 be", "8b 88 ? ? ? ? 8b 90 ? ? ? ? 51 52 8b ce", "8b 91 ? ? ? ? 8b 4e ? 2b d1", "8b 82 ? ? ? ? 89 8e ? ? ? ? 99 2b c2 89 8e ? ? ? ? d1 f8 83 e8 ? 89 8e ? ? ? ? 89 86 ? ? ? ? b8 ? ? ? ? 89 8e ? ? ? ? 89 8e ? ? ? ? 89 8e", "8b 82 ? ? ? ? 89 8e ? ? ? ? 99 2b c2 89 8e ? ? ? ? d1 f8 83 e8 ? 89 8e ? ? ? ? 89 86 ? ? ? ? b8 ? ? ? ? 89 8e ? ? ? ? 2b f8", "8b 82 ? ? ? ? 89 8e ? ? ? ? 99 2b c2 89 8e ? ? ? ? d1 f8 83 e8 ? 89 8e ? ? ? ? 89 86 ? ? ? ? b8 ? ? ? ? 89 8e ? ? ? ? 89 8e ? ? ? ? c7 86", "8b 81 ? ? ? ? 89 9e", "8b 88 ? ? ? ? 8b 90 ? ? ? ? 8b 46 ? c1 f9 ? c1 fa ? 51 8b 8e ? ? ? ? 52 50 e8 ? ? ? ? 8b 86", "8b 88 ? ? ? ? 8b 44 24", "8b 88 ? ? ? ? 2b ca", "8b 90 ? ? ? ? 8b 80 ? ? ? ? 6a ? 6a", "8b 88 ? ? ? ? 8b 90 ? ? ? ? 6a", "8b 81 ? ? ? ? 99 2b c2 8d 96 ? ? ? ? d1 f8 83 e8 ? 89 86 ? ? ? ? b8 ? ? ? ? 2b d0 8a 08 88 0c ? 40 3a cb 75 ? 68 ? ? ? ? 8b ce e8 ? ? ? ? 8b 4c 24 ? 89 9e ? ? ? ? 89 9e ? ? ? ? 8b c6 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c2 ? ? 90 90 90 90 90 90 90 90 90 56 8b f1 e8 ? ? ? ? f6 44 24 ? ? 74 ? 56 e8 ? ? ? ? 83 c4 ? 8b c6 5e c2 ? ? 90 90 6a ? 68 ? ? ? ? 64 a1 ? ? ? ? 50 64 89 25 ? ? ? ? 51 53 56 8b f1 57 89 74 24 ? c7 06 ? ? ? ? 8b 86 ? ? ? ? 8d be ? ? ? ? 8b cf c7 44 24 ? ? ? ? ? ff 50 ? 8b 56 ? 8d 5e ? 8b cb ff 52 ? 8b cf c7 44 24 ? ? ? ? ? e8 ? ? ? ? 8b cb c6 44 24 ? ? e8 ? ? ? ? 8b 4c 24 ? c7 06 ? ? ? ? 5f 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c3 90 90 90 90 64 a1 ? ? ? ? 6a ? 68 ? ? ? ? 50 8b 44 24 ? 64 89 25 ? ? ? ? 81 ec ? ? ? ? 55 56 8b f1 33 ed 3b c5 89 86 ? ? ? ? 89 ae ? ? ? ? 89 ae ? ? ? ? 75 ? 8b 46 ? 8d 8e ? ? ? ? 24 ? 89 46 ? 8b 01 ff 50 ? e9 ? ? ? ? 8b 0d", "8b 81 ? ? ? ? 99 2b c2 8b d6", "8b 81 ? ? ? ? 99 2b c2 8d 96 ? ? ? ? d1 f8 83 e8 ? 89 86 ? ? ? ? b8 ? ? ? ? 2b d0 8a 08 88 0c ? 40 3a cb 75 ? 68 ? ? ? ? 8b ce e8 ? ? ? ? 8b 4c 24 ? 89 9e ? ? ? ? 89 9e ? ? ? ? 8b c6 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c2 ? ? 90 90 90 90 90 90 90 90 90 56 8b f1 e8 ? ? ? ? f6 44 24 ? ? 74 ? 56 e8 ? ? ? ? 83 c4 ? 8b c6 5e c2 ? ? 90 90 6a ? 68 ? ? ? ? 64 a1 ? ? ? ? 50 64 89 25 ? ? ? ? 51 53 56 8b f1 57 89 74 24 ? c7 06 ? ? ? ? 8b 86 ? ? ? ? 8d be ? ? ? ? 8b cf c7 44 24 ? ? ? ? ? ff 50 ? 8b 56 ? 8d 5e ? 8b cb ff 52 ? 8b cf c7 44 24 ? ? ? ? ? e8 ? ? ? ? 8b cb c6 44 24 ? ? e8 ? ? ? ? 8b 4c 24 ? c7 06 ? ? ? ? 5f 5e 5b 64 89 0d ? ? ? ? 83 c4 ? c3 90 90 90 90 64 a1 ? ? ? ? 6a ? 68 ? ? ? ? 50 8b 44 24 ? 64 89 25 ? ? ? ? 81 ec ? ? ? ? 55 56 8b f1 33 ed 3b c5 89 86 ? ? ? ? 89 ae ? ? ? ? 89 ae ? ? ? ? 75 ? 8b 46 ? 8d 8e ? ? ? ? 24 ? 89 46 ? 8b 01 ff 50 ? e9 ? ? ? ? 8b 8c 24", "8b 81 ? ? ? ? 99 2b c2 8d 96 ? ? ? ? d1 f8 83 e8 ? 89 86 ? ? ? ? b8 ? ? ? ? 2b d0 8a 08 88 0c ? 40 3a cb 75 ? 68 ? ? ? ? 8b ce e8 ? ? ? ? 8b 4c 24 ? 8b c6", "8b 91 ? ? ? ? 55", "8b 8d ? ? ? ? 8b 15 ? ? ? ? d1 f9 2b c8 8b 42", "8b 82 ? ? ? ? d1 f8 89 86", "8b 97 ? ? ? ? d1 fa 2b d0 8b 84 24", "8b 8b ? ? ? ? d1 f9", "8b 8f ? ? ? ? 8b 94 24",
		"db 80 ? ? ? ? da 64 24", "db 86 ? ? ? ? d9 1c", "db 86 ? ? ? ? d8 0d ? ? ? ? d9 96 ? ? ? ? dd 5c 24", "db 81 ? ? ? ? d8 a6", "db 81 ? ? ? ? d8 64 24", "db 80 ? ? ? ? db 80 ? ? ? ? 8b 46 ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 80 ? ? ? ? 50 e8 ? ? ? ? 83 c4 ? 8b 56", "db 80 ? ? ? ? db 80 ? ? ? ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 82 ? ? ? ? 50 e8 ? ? ? ? 8b 0d", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 d9 c9 d8 8f ? ? ? ? de c1 e8 ? ? ? ? 89 44 24 ? db 43 ? d8 8f ? ? ? ? e8 ? ? ? ? db 44 24 ? 89 44 24 ? d8 8f ? ? ? ? e8 ? ? ? ? 8b 8e ? ? ? ? 89 44 24 ? 8b 43 ? 8d 54 24 ? 52 50 51 e8 ? ? ? ? 8b 86 ? ? ? ? 8d 54 24 ? 52 50 e8 ? ? ? ? 83 c4 ? a1 ? ? ? ? 3b c5 7e ? 50 8d 8c 24 ? ? ? ? 68 ? ? ? ? 51 e8 ? ? ? ? 8b 3d ? ? ? ? 83 c4 ? 8d 94 24 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 55 6a ? 6a ? 55 de e9 6a ? 52 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? d9 05", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 55 6a ? 6a ? 55 de e9 6a ? 52 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? d9 05", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? d9 05", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 d9 c9 d8 8f ? ? ? ? de c1 e8 ? ? ? ? 89 44 24 ? db 43 ? d8 8f ? ? ? ? e8 ? ? ? ? db 44 24 ? 89 44 24 ? d8 8f ? ? ? ? e8 ? ? ? ? 8b 8e ? ? ? ? 89 44 24 ? 8b 43 ? 8d 54 24 ? 52 50 51 e8 ? ? ? ? 8b 86 ? ? ? ? 8d 54 24 ? 52 50 e8 ? ? ? ? 83 c4 ? a1 ? ? ? ? 3b c5 7e ? 50 8d 8c 24 ? ? ? ? 68 ? ? ? ? 51 e8 ? ? ? ? 8b 3d ? ? ? ? 83 c4 ? 8d 94 24 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 55 6a ? 6a ? 55 de e9 6a ? 52 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 8b 8e", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 55 6a ? 6a ? 55 de e9 6a ? 52 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b ce e8 ? ? ? ? 8b 3d ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 8b 8e", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 8b 8e", "db 80 ? ? ? ? 8b 90", "db 80 ? ? ? ? d9 80", "db 83 ? ? ? ? d9 83", "db 80 ? ? ? ? db 80 ? ? ? ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 82 ? ? ? ? 50 e8 ? ? ? ? 83 c4 ? eb ? 55", "db 80 ? ? ? ? db 80 ? ? ? ? 51 de f9 d8 0d ? ? ? ? d9 5c 24 ? 8b 82 ? ? ? ? 50 e8 ? ? ? ? 83 c4 ? eb ? 8b 8e", "db 80 ? ? ? ? 51 52", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 d9 c9 d8 8f ? ? ? ? de c1 e8 ? ? ? ? 89 44 24 ? db 43 ? d8 8f ? ? ? ? e8 ? ? ? ? db 44 24 ? 89 44 24 ? d8 8f ? ? ? ? e8 ? ? ? ? 89 44 24", "db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? de e9 e8 ? ? ? ? db 87 ? ? ? ? d9 87 ? ? ? ? d8 0d ? ? ? ? 50 de e9 e8 ? ? ? ? 50 8b 86 ? ? ? ? 50 e8 ? ? ? ? 83 c4");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "c7 44 24 ? ? ? ? ? 8b 53", "d8 0d ? ? ? ? d9 5c 24 ? 8b 82 ? ? ? ? 50 e8 ? ? ? ? 8b 0d");

		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport6Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[RendererScan], 2);

			RendererResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererScan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[RendererScan] + 8, 3);

			RendererResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererScan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport1Scan], 8);

			ViewportResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport1Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport2Scan], 8);

			ViewportResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport2Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport3Scan], 3);

			ViewportResolutionWidthInstruction3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport3Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport3Scan] + 7, 3);

			ViewportResolutionHeightInstruction3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport3Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport4Scan], 3);

			ViewportResolutionWidthInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport4Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport4Scan] + 33, 3);

			ViewportResolutionHeightInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport4Scan] + 33, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport5Scan], 3);

			ViewportResolutionWidthInstruction5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport5Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport5Scan] + 7, 3);

			ViewportResolutionHeightInstruction5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport5Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport6Scan], 3);

			ViewportResolutionWidthInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport6Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport6Scan] + 7, 3);

			ViewportResolutionHeightInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport6Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			NewResXPtr = &iCurrentResX;

			NewResYPtr = &iCurrentResY;

			Memory::Write(ResolutionInstructionsScansResult, Viewport7Scan, Viewport53Scan, 2, NewResXPtr);

			Memory::Write(ResolutionInstructionsScansResult, Viewport54Scan, Viewport98Scan, 2, NewResYPtr);

			auto ProcessScan = [&](std::size_t i)
			{
				std::uint8_t* address = ResolutionInstructionsScansResult[i];

				if (!address)
				{
					return;
				}						

				std::uint8_t opcode = address[1];

				switch (opcode)
				{
					// EAX mov opcodes
					case 0x80:
					case 0x81:
					case 0x82:
					case 0x87:
					{
						Memory::PatchBytes(address + 1, "\x05");
						break;
					}

					// ECX mov opcodes
					case 0x88:
					case 0x8B:
					case 0x8D:
					case 0x8F:
					{
						Memory::PatchBytes(address + 1, "\x0D");
						break;
					}

					// EDX mov opcodes
					case 0x90:
					case 0x91:
					case 0x97:
					{
						Memory::PatchBytes(address + 1, "\x15");
						break;
					}
				}
			};

			for (std::size_t i = Viewport7Scan; i <= Viewport38Scan; ++i)
			{
				ProcessScan(i);
			}

			for (std::size_t i = Viewport54Scan; i <= Viewport77Scan; ++i)
			{
				ProcessScan(i);
			}

			Memory::PatchBytes(ResolutionInstructionsScansResult, Viewport39Scan, Viewport53Scan, 1, "\x05");

			Memory::PatchBytes(ResolutionInstructionsScansResult, Viewport78Scan, Viewport98Scan, 1, "\x05");
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOVScan] - (std::uint8_t*)exeModule);

			fNewCameraHFOV = (0.5f * fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[HFOVScan] + 4, fNewCameraHFOV);

			CameraVFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[VFOVScan] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOVScan], 6);
			
			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(CameraVFOVAddress);

				fNewCameraVFOV = (fCurrentCameraVFOV * fAspectRatioScale) * fFOVFactor;

				FPU::FMUL(fNewCameraVFOV);
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