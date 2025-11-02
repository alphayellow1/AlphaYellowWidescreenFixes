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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "ViperRacingWidescreenFix";
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

// Game detection
enum class Game
{
	VR,
	Unknown
};

enum ResolutionListsScans
{
	ResolutionListScan,
	ViewportResolutions1Scan,
	ViewportResolutions2Scan,
	ViewportResolutions3Scan,
	ViewportResolutions4Scan,
	ViewportResolutions5Scan,
	ViewportResolutions6Scan,
	ViewportResolutions7Scan,
	ViewportResolutions8Scan,
	ViewportResolutions9Scan,
	ViewportResolutions10Scan,
	ViewportResolutions11Scan,
	ViewportResolutions12Scan,
	ViewportResolutions13Scan,
	ViewportResolutions14Scan,
	ViewportResolutions15Scan,
	ViewportResolutions16Scan,
	ViewportResolutions17Scan,
	ViewportResolutions18Scan,
	ViewportResolutions19Scan,
	ViewportResolutions20Scan,
	ViewportResolutions21Scan,
	ViewportResolutions22Scan,
	ViewportResolutions23Scan,
	ViewportResolutions24Scan,
	ViewportResolutions25Scan,
	ViewportResolutions26Scan,
	ViewportResolutions27Scan,
	ViewportResolutions28Scan,
	ViewportResolutions29Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::VR, {"Viper Racing", "race.exe"}},
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

static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid ViewportResolutions1Hook{};
static SafetyHookMid ViewportResolutionWidth2Hook{};
static SafetyHookMid ViewportResolutionHeight2Hook{};
static SafetyHookMid ViewportResolutionWidth3Hook{};
static SafetyHookMid ViewportResolutionHeight3Hook{};
static SafetyHookMid ViewportResolutionWidth4Hook{};
static SafetyHookMid ViewportResolutionHeight4Hook{};
static SafetyHookMid ViewportResolutionWidth5Hook{};
static SafetyHookMid ViewportResolutionHeight5Hook{};
static SafetyHookMid ViewportResolutionWidth6Hook{};
static SafetyHookMid ViewportResolutionHeight6Hook{};
static SafetyHookMid ViewportResolutionWidth7Hook{};
static SafetyHookMid ViewportResolutionHeight7Hook{};
static SafetyHookMid ViewportResolutionWidth8Hook{};
static SafetyHookMid ViewportResolutionHeight8Hook{};
static SafetyHookMid ViewportResolutionWidth9Hook{};
static SafetyHookMid ViewportResolutionHeight9Hook{};
static SafetyHookMid ViewportResolutionInstructions9Hook{};
static SafetyHookMid ViewportResolutions10Hook{};
static SafetyHookMid ViewportResolutionWidth11Hook{};
static SafetyHookMid ViewportResolutionHeight11Hook{};
static SafetyHookMid ViewportResolutionWidth12Hook{};
static SafetyHookMid ViewportResolutionHeight12Hook{};
static SafetyHookMid ViewportResolutionWidth13Hook{};
static SafetyHookMid ViewportResolutionHeight13Hook{};
static SafetyHookMid ViewportResolutionWidth14Hook{};
static SafetyHookMid ViewportResolutionHeight14Hook{};
static SafetyHookMid ViewportResolutionWidth15Hook{};
static SafetyHookMid ViewportResolutionHeight15Hook{};
static SafetyHookMid ViewportResolutionWidth16Hook{};
static SafetyHookMid ViewportResolutionHeight16Hook{};
static SafetyHookMid ViewportResolutionWidth17Hook{};
static SafetyHookMid ViewportResolutionHeight17Hook{};
static SafetyHookMid ViewportResolutionWidth18Hook{};
static SafetyHookMid ViewportResolutionWidth19Hook{};
static SafetyHookMid ViewportResolutionWidth20Hook{};
static SafetyHookMid ViewportResolutionHeight20Hook{};
static SafetyHookMid ViewportResolutionWidth21Hook{};
static SafetyHookMid ViewportResolutionHeight21Hook{};
static SafetyHookMid ViewportResolutionHeight22Hook{};
static SafetyHookMid ViewportResolutionHeight23Hook{};

void WidescreenFix()
{
	if (eGameType == Game::VR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "B8 00 02 00 00 B9 80 01 00 00 C6 05 20 2B 52 00 04 EB 37 B8 80 02 00 00 B9 E0 01 00 00 C6 05 20 2B 52 00 04 EB 24 B8 20 03 00 00 B9 58 02 00 00 C6 05 20 2B 52 00 04 EB 11 B8 00 04 00 00 B9 00 03 00 00", "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 6A ?? 8D 44 24 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 52 50 E8 ?? ?? ?? ?? 83 C4 ?? 83 F8", "A1 ?? ?? ?? ?? 2B C5 99", "8B 35 ?? ?? ?? ?? 6A ?? 57", "A1 ?? ?? ?? ?? 99 55", "A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 54 24 ?? 68 ?? ?? ?? ?? 50 51 52 E8 ?? ?? ?? ?? 83 C4 ?? 8B F8", "8B 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8D 4C 24 ?? 68 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 52 50 51 E8 ?? ?? ?? ?? 83 C4 ?? 83 C0", "8B 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8D 4C 24 ?? 68 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 52 50 51 E8 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 8B 4C 24", "A1 ?? ?? ?? ?? 50 8B 0D ?? ?? ?? ?? 51 53", "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 80 BC 24", "A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 50", "A1 ?? ?? ?? ?? 99 2B C2 C1 F8 ?? 2B C8 89 4C 24", "A1 ?? ?? ?? ?? B9 ?? ?? ?? ?? 99", "A1 ?? ?? ?? ?? 99 2B C2 C1 F8 ?? 8B D8", "A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 54 24 ?? 68 ?? ?? ?? ?? 50 51 52 E8 ?? ?? ?? ?? 83 C4 ?? 85 C0", "A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 51 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 4C 24", "8B 1D ?? ?? ?? ?? 6A ?? 55", "A1 ?? ?? ?? ?? 2D ?? ?? ?? ?? 99 2B C2 C1 F8 ?? 89 44 24", "A1 ?? ?? ?? ?? 2D ?? ?? ?? ?? 53", "A1 ?? ?? ?? ?? 6A ?? 2D ?? ?? ?? ?? 99", "A1 ?? ?? ?? ?? 50 8B 15", "A1 ?? ?? ?? ?? 50 8B 0D ?? ?? ?? ?? 80 3D", "A1 ?? ?? ?? ?? 6A ?? 2D", "A1 ?? ?? ?? ?? 51 68 ?? ?? ?? ?? 83 E8", "03 05 ?? ?? ?? ?? 99", "A1 ?? ?? ?? ?? 83 E8 ?? 50 6A", "8B 3D ?? ?? ?? ?? 56 2B FD", "8B 15 ?? ?? ?? ?? 51 52 6A", "8B 3D ?? ?? ?? ?? 53 2B F8", "8B 3D ?? ?? ?? ?? A1 ?? ?? ?? ?? 8D 4F", "A1 ?? ?? ?? ?? 99 2B C2 C1 F8 ?? 50 A1");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResolutionListScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions9Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions10Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 11 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions11Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 12 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions12Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 13 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions13Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 14 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions14Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 15 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions15Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 16 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions16Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 17 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions17Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 18 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions18Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 19 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions19Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 20 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions20Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 21 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions21Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 22 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions22Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 23 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions23Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 24 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions24Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 25 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions25Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 26 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions26Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 27 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions27Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 28 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions28Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolutions 29 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutions29Scan] - (std::uint8_t*)exeModule);

			// Resolution List			
			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 1, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 6, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 20, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 25, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 39, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 44, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 58, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 63, iCurrentResY);

			// Viewport Resolutions 1
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions1Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions1Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 2
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions2Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions2Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions2Scan] + 27, "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions2Scan] + 27, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 3
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions3Scan] + 21, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions3Scan] + 21, [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeight3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions3Scan], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 4
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions4Scan] + 16, "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions4Scan] + 16, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions4Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions4Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 5
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions5Scan] + 10, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions5Scan] + 10, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions5Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions5Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			// Viewport Resolutions 6
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions6Scan] + 11, "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions6Scan] + 11, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeight6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions6Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 7
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions7Scan] + 11, "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth7Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions7Scan] + 11, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});
			
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions7Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeight7Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions7Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 8
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions8Scan] + 6, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth8Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions8Scan] + 6, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions8Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight8Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions8Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});			
			
			// Viewport Resolutions 9
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions9Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			ViewportResolutionInstructions9Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions9Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});			
			
			// Viewport Resolutions 10
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions10Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			ViewportResolutions10Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions10Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});			
			
			// Viewport Resolutions 11
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions11Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth11Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions11Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions11Scan] + 25, "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight11Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions11Scan] + 25, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			// Viewport Resolutions 12
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions12Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth12Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions12Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions12Scan] + 18, "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight12Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions12Scan] + 18, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
						
			// Viewport Resolutions 13
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions13Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight13Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions13Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			// Viewport Resolutions 14
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions14Scan] + 10, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth14Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions14Scan] + 10, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions14Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight14Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions14Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			// Viewport Resolutions 15
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions15Scan] + 10, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth15Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions15Scan] + 10, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions15Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight15Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions15Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			// Viewport Resolutions 16
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions16Scan] + 21, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth16Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions16Scan] + 21, [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions16Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeight16Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions16Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 17
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions17Scan] + 20, "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth17Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions17Scan] + 20, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions17Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight17Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions17Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 18
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions18Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth18Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions18Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			// Viewport Resolutions 19
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions19Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionWidth19Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions19Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			// Viewport Resolutions 20
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions20Scan] + 6, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth20Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions20Scan] + 6, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions20Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight20Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions20Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 21
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions21Scan] + 6, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidth21Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions21Scan] + 6, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions21Scan], "\x90\x90\x90\x90\x90", 5);
			
			ViewportResolutionHeight21Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions21Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 22
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions22Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight22Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions22Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			// Viewport Resolutions 23
			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutions23Scan], "\x90\x90\x90\x90\x90", 5);

			ViewportResolutionHeight23Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutions23Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			int* NewHorizontalResPtr = &iCurrentResX;

			int* NewVerticalResPtr = &iCurrentResY;

			// Viewport Resolutions 24
			Memory::Write(ResolutionInstructionsScansResult[ViewportResolutions24Scan] + 2, NewVerticalResPtr);

			// Viewport Resolutions 25
			// Memory::Write(ResolutionInstructionsScansResult[ViewportResolutions25Scan] + 1, NewVerticalResPtr);

			// Viewport Resolutions 26
			Memory::Write(ResolutionInstructionsScansResult[ViewportResolutions26Scan] + 2, NewHorizontalResPtr);

			// Viewport Resolutions 27
			Memory::Write(ResolutionInstructionsScansResult[ViewportResolutions27Scan] + 2, NewHorizontalResPtr);

			// Viewport Resolutions 28
			Memory::Write(ResolutionInstructionsScansResult[ViewportResolutions28Scan] + 2, NewVerticalResPtr);

			// Viewport Resolutions 29
			Memory::Write(ResolutionInstructionsScansResult[ViewportResolutions29Scan] + 1, NewVerticalResPtr);
		}
		
		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 ?? D8 0D ?? ?? ?? ?? 83 3D ?? ?? ?? ?? ?? 75 ?? D9 F2 EB ?? E8 ?? ?? ?? ?? DD D8");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90", 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0xC);

				fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, 1.0f / fAspectRatioScale) / fFOVFactor;

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction scan memory address.");
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
