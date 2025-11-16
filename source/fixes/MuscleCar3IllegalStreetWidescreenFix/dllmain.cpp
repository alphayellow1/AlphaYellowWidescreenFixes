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
std::string sFixName = "MuscleCar3IllegalStreetWidescreenFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalCameraHFOV = 0.7874999642372131f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fNewGameplayCameraHFOV;
float fNewMenuCameraHFOV;
float fNewGameplayCameraVFOV;
float fNewMenuCameraVFOV;
uint8_t* CameraVFOVAddress;

// Game detection
enum class Game
{
	MC3IS,
	Unknown
};

enum ResolutionInstructionsIndex
{
	RendererResolutionScan,
	ViewportResolution1Scan,
	ViewportResolution2Scan,
	ViewportResolution3Scan,
	ViewportResolution4Scan,
	ViewportResolution5Scan,
	ViewportResolution6Scan,
	ViewportResolution7Scan,
	ViewportResolution8Scan,
	ViewportResolution9Scan,
	ViewportResolution10Scan,
	ViewportResolution11Scan,
	ViewportResolution12Scan,
	ViewportResolution13Scan,
	ViewportResolution14Scan,
	ViewportResolution15Scan,
	ViewportResolution16Scan,
	ViewportResolution17Scan,
	ViewportResolution18Scan,
	ViewportResolution19Scan,
	ViewportResolution20Scan,
	ViewportResolution21Scan,
	ViewportResolution22Scan,
	ViewportResolution23Scan,
	ViewportResolution24Scan,
	ViewportResolution25Scan,
	ViewportResolution26Scan,
	ViewportResolution27Scan,
	ViewportResolution28Scan,
	ViewportResolution29Scan,
	ViewportResolution30Scan,
	ViewportResolution31Scan,
	ViewportResolution32Scan,
	ViewportResolution33Scan,
	ViewportResolution34Scan,
	ViewportResolution35Scan,
	ViewportResolution36Scan,
	ViewportResolution37Scan,
	ViewportResolution38Scan,
	ViewportResolution39Scan,
	ViewportResolution40Scan,
	ViewportResolution41Scan,
	ViewportResolution42Scan,
	ViewportResolution43Scan,
	ViewportResolution44Scan,
	ViewportResolution45Scan,
	ViewportResolution46Scan,
	ViewportResolution47Scan,
	ViewportResolution48Scan,
	ViewportResolution49Scan,
	ViewportResolution50Scan,
	ViewportResolution51Scan,
	ViewportResolution52Scan,
	ViewportResolution53Scan,
	ViewportResolution54Scan,
	ViewportResolution55Scan,
	ViewportResolution56Scan,
	ViewportResolution57Scan,
	ViewportResolution58Scan,
	ViewportResolution59Scan,
	ViewportResolution60Scan,
	ViewportResolution61Scan,
	ViewportResolution62Scan,
	ViewportResolution63Scan,
	ViewportResolution64Scan,
	ViewportResolution65Scan,
	ViewportResolution66Scan,
	ViewportResolution67Scan,
	ViewportResolution68Scan,
	ViewportResolution69Scan,
	ViewportResolution70Scan,
	ViewportResolution71Scan,
	ViewportResolution72Scan,
	ViewportResolution73Scan,
	ViewportResolution74Scan,
	ViewportResolution75Scan,
	ViewportResolution76Scan,
	ViewportResolution77Scan,
	ViewportResolution78Scan,
	ViewportResolution79Scan,
	ViewportResolution80Scan,
	ViewportResolution81Scan,
	ViewportResolution82Scan,
	ViewportResolution83Scan,
	ViewportResolution84Scan,
	ViewportResolution85Scan,
	ViewportResolution86Scan,
	ViewportResolution87Scan,
	ViewportResolution88Scan,
	ViewportResolution89Scan,
	ViewportResolution90Scan,
	ViewportResolution91Scan,
	ViewportResolution92Scan,
	ViewportResolution93Scan,
	ViewportResolution94Scan,
	ViewportResolution95Scan,
	ViewportResolution96Scan,
	ViewportResolution97Scan,
	ViewportResolution98Scan,
	ViewportResolution99Scan,
	ViewportResolution100Scan,
	ViewportResolution101Scan,
	ViewportResolution102Scan,
	ViewportResolution103Scan,
	ViewportResolution104Scan,
	ViewportResolution105Scan,
	ViewportResolution106Scan,
	ViewportResolution107Scan,
	ViewportResolution108Scan,
	ViewportResolution109Scan,
	ViewportResolution110Scan,
	ViewportResolution111Scan,
	ViewportResolution112Scan,
	ViewportResolution113Scan,
	ViewportResolution114Scan,
	ViewportResolution115Scan,
	ViewportResolution116Scan,
	ViewportResolution117Scan,
	ViewportResolution118Scan,
	ViewportResolution119Scan,
	ViewportResolution120Scan,
	ViewportResolution121Scan,
	ViewportResolution122Scan,
	ViewportResolution123Scan,
	ViewportResolution124Scan,
	ViewportResolution125Scan,
	ViewportResolution126Scan,
	ViewportResolution127Scan,
	ViewportResolution128Scan,
	ViewportResolution129Scan,
	ViewportResolution130Scan,
	ViewportResolution131Scan,
	ViewportResolution132Scan,
	ViewportResolution133Scan,
	ViewportResolution134Scan,
	ViewportResolution135Scan,
	ViewportResolution136Scan,
	ViewportResolution137Scan,
	ViewportResolution138Scan,
	ViewportResolution139Scan,
	ViewportResolution140Scan,
	ViewportResolution141Scan,
	ViewportResolution142Scan,
	ViewportResolution143Scan,
	ViewportResolution144Scan,
	ViewportResolution145Scan,
	ViewportResolution146Scan,
	ViewportResolution147Scan,
	ViewportResolution148Scan,
	ViewportResolution149Scan,
	ViewportResolution150Scan,
	ViewportResolution151Scan,
	ViewportResolution152Scan,
	ViewportResolution153Scan,
	ViewportResolution154Scan,
	ViewportResolution155Scan,
	ViewportResolution156Scan,
	ViewportResolution157Scan
};

enum CameraFOVInstructionsIndex
{
	GameplayCameraFOVScan,
	MenuCameraFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MC3IS, {"Muscle Car 3: Illegal Street", "MC3.exe"}},
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
static SafetyHookMid ViewportResolutionWidthInstruction1Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction1Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};
static SafetyHookMid ViewportResolutionInstructions3Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction4Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction4Hook{};
static SafetyHookMid ViewportResolutionInstructions5Hook{};
static SafetyHookMid ViewportResolutionInstructions6Hook{};
static SafetyHookMid ViewportResolutionInstructions7Hook{};
static SafetyHookMid ViewportResolutionInstructions8Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction9Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction10Hook{};
static SafetyHookMid ViewportResolutionInstructions11Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction12Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction13Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction13Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction14Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction14Hook{};
static SafetyHookMid ViewportResolutionInstructions15Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction16Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction16Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction17Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction17Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction18Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction18Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction19Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction19Hook{};
static SafetyHookMid ViewportResolutionInstructions20Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction21Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction22Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction23Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction24Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction25Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction26Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction27Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction28Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction29Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction30Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction31Hook{};
static SafetyHookMid ViewportResolutionInstructions32Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction33Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction34Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction35Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction36Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction37Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction38Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction39Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction40Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction41Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction42Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction43Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction44Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction44Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction45Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction45Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction46Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction47Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction48Hook{};
static SafetyHookMid ViewportResolutionInstructions49Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction50Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction51Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction51Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction52Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction52Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction53Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction53Hook{};
static SafetyHookMid ViewportResolutionInstructions54Hook{};
static SafetyHookMid ViewportResolutionInstructions55Hook{};
static SafetyHookMid ViewportResolutionInstructions56Hook{};
static SafetyHookMid ViewportResolutionInstructions57Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction58Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction59Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction60Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction61Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction61Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction62Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction62Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction63Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction64Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction64Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction65Hook{};
static SafetyHookMid ViewportResolutionInstructions66Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction67Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction68Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction69Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction70Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction70Hook{};
static SafetyHookMid ViewportResolutionInstructions71Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction72Hook{};
static SafetyHookMid ViewportResolutionInstructions73Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction74Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction75Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction76Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction76Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction77Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction77Hook{};
static SafetyHookMid ViewportResolutionInstructions78Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction79Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction79Hook{};
static SafetyHookMid ViewportResolutionInstructions80Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction81Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction82Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction83Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction84Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction85Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction86Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction87Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction88Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction89Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction90Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction91Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction92Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction93Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction94Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction95Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction96Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction97Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction98Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction99Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction100Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction101Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction102Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction103Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction104Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction105Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction106Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction107Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction108Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction108Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction109Hook{};
static SafetyHookMid ViewportResolutionInstructions110Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction111Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction112Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction113Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction114Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction115Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction116Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction117Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction118Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction119Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction120Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction121Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction122Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction123Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction124Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction125Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction126Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction127Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction128Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction129Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction130Hook{};
static SafetyHookMid ViewportResolutionInstructions131Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction132Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction133Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction134Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction135Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction136Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction137Hook{};
static SafetyHookMid ViewportResolutionInstructions138Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction139Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction140Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction141Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction142Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction143Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction144Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction145Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction146Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction147Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction148Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction149Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction150Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction151Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction152Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction153Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction154Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction155Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction156Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction157Hook{};
static SafetyHookMid GameplayCameraVFOVInstructionHook{};
static SafetyHookMid MenuCameraVFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::MC3IS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0F 89 0D", "66 8B 4D ?? 8D 44 24", "8B 4E ?? 89 54 24 ?? 8B 56 ?? 89 44 24 ?? A1", "8B 71 ?? 8B 69", "8B 4D ?? A1 ?? ?? ?? ?? 89 4C 24", "8B 5E ?? 8B 6E ?? 89 54 24", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 6A ?? 6A", "8B 88 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 81 E9", "8B 88 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 83 E9 ?? 83 EA ?? 51 52 8B CE E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 83 E9 ?? 89 4C 24 ?? DB 44 24 ?? EB", "8B 88 ?? ?? ?? ?? 83 E9 ?? 89 4C 24 ?? DB 44 24 ?? EB", "8B 82 ?? ?? ?? ?? 83 E8", "8B 88 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 8B 86", "DB 82 ?? ?? ?? ?? D8 64 24 ?? D9 1C ?? E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 8B 8E ?? ?? ?? ?? 50 51 E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 8B 87", "8B 82 ?? ?? ?? ?? 99 2B C2 D1 F8 2D", "8B 88 ?? ?? ?? ?? D1 F9 81 E9 ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 82 ?? ?? ?? ?? D1 F8 2D ?? ?? ?? ?? 89 86 ?? ?? ?? ?? E8", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 52", "DB 86 ?? ?? ?? ?? D9 1C", "DB 80 ?? ?? ?? ?? 56", "8B 80 ?? ?? ?? ?? C6 44 24", "8B 88 ?? ?? ?? ?? D1 F9 81 E9 ?? ?? ?? ?? 89 8D", "8B 88 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 8B 46 ?? D1 F9 D1 FA 81 E9 ?? ?? ?? ?? 81 EA ?? ?? ?? ?? 51 52 50 E8 ?? ?? ?? ?? 8B 0E 8B 81 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 0F 84 ?? ?? ?? ?? 57", "8B 82 ?? ?? ?? ?? D1 F8 89 44 24", "8B 90 ?? ?? ?? ?? 6A ?? 3B CB", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 6A ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 06", "8B 88 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 8B 46 ?? D1 F9 D1 FA 81 E9 ?? ?? ?? ?? 81 EA ?? ?? ?? ?? 51 52 50 E8 ?? ?? ?? ?? 8B 0E 8B 81 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 46", "8B 91 ?? ?? ?? ?? D1 FA", "8B 90 ?? ?? ?? ?? 6A ?? 6A", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 6A ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? 8B 0D", "8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? E8", "DB 81 ?? ?? ?? ?? D9 1C", "8B 80 ?? ?? ?? ?? 68 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 83 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 82 ?? ?? ?? ?? 89 8B ?? ?? ?? ?? 99", "8B 81 ?? ?? ?? ?? D8 0D", "DB 80 ?? ?? ?? ?? DA 64 24", "8B 91 ?? ?? ?? ?? 8B 4C 24 ?? 2B D1", "8B 88 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 8B 86", "8B 8A ?? ?? ?? ?? 81 E9 ?? ?? ?? ?? 51 6A ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B 83 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 83 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 6C 24 ?? 8B 83 ?? ?? ?? ?? 81 CD ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 6C 24 ?? 8B B0 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 83 C6 ?? B9 ?? ?? ?? ?? 8D 7C 24 ?? F3 ?? 8B 8C 24 ?? ?? ?? ?? 85 C9 74 ?? D9 41 ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 41 ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 41 ?? EB ?? 8B 80 ?? ?? ?? ?? 83 C2 ?? 3B D0 7E ?? 2B D0 8B 8B ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 83 C2 ?? 50 52 E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 84 24 ?? ?? ?? ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 84 24 ?? ?? ?? ?? D8 A4 24 ?? ?? ?? ?? 8D 4C 24 ?? 8D 54 24 ?? 51 52 D9 5C 24 ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? DD D8 D9 05 ?? ?? ?? ?? D9 E0 89 4C 24 ?? 89 54 24 ?? D9 44 24 ?? D9 E0 D9 44 24 ?? D9 E0 D9 C0 D8 4C 24 ?? A1 ?? ?? ?? ?? 89 44 24 ?? D9 C2 D8 4C 24 ?? DE E9 D9 5C 24 ?? D9 C2 D8 4C 24 ?? D9 C9 D8 4C 24 ?? DE E9 D9 5C 24 ?? D8 4C 24 ?? D9 C9 D8 4C 24 ?? DE E9 D9 54 24 ?? D8 4C 24 ?? D9 44 24 ?? D8 4C 24 ?? DE E9 D9 5C 24 ?? D9 44 24 ?? D8 4C 24 ?? D9 44 24 ?? D8 4C 24 ?? DE E9 D9 5C 24 ?? D9 44 24 ?? D8 4C 24 ?? D9 44 24 ?? D8 4C 24 ?? DE E9 8D 44 24 ?? 8D 4C 24 ?? 50 51 D9 5C 24 ?? E8 ?? ?? ?? ?? 8D 54 24 ?? 8D 44 24 ?? 52 50 DD D8 E8 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 6A ?? 8D 54 24 ?? 51 52 DD D8 E8 ?? ?? ?? ?? 8B 44 24 ?? 8B 4C 24 ?? 8B 54 24 ?? 89 84 24 ?? ?? ?? ?? 8B 44 24 ?? 89 8C 24 ?? ?? ?? ?? 8B 4C 24 ?? 89 44 24 ?? 8B 44 24 ?? 89 94 24 ?? ?? ?? ?? 8B 54 24 ?? 89 44 24 ?? 89 4C 24 ?? 8B 4C 24 ?? 89 54 24 ?? 8B 54 24 ?? 8D 44 24 ?? 89 4C 24 ?? 50 89 54 24 ?? E8 ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? D9 44 24 ?? D8 64 24 ?? 83 C4 ?? D9 1C ?? E8 ?? ?? ?? ?? 8B 9B ?? ?? ?? ?? 8B 4B ?? 8B 53 ?? 51 52 E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? 90 90 90 56", "8B 88 ?? ?? ?? ?? D1 F9 81 E9 ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 82 ?? ?? ?? ?? D1 F8 2D ?? ?? ?? ?? 89 86 ?? ?? ?? ?? A1", "8B 80 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 86 ?? ?? ?? ?? A1 ?? ?? ?? ?? 3B C5", "DB 80 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? D9 1C ?? DB 80 ?? ?? ?? ?? 51 D8 25 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? 8B 8E", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 6A ?? 55 6A ?? 83 EA ?? 68 ?? ?? ?? ?? D1 F8 52 50 E8 ?? ?? ?? ?? 5F", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 6A ?? 53 6A ?? 83 EA ?? 68 ?? ?? ?? ?? D1 F8 52 50 E8 ?? ?? ?? ?? 5E 5D", /* testing => */ "89 BE D0 00 00 00 89 BE D4 00 00 00 89 7E 08 89 3D 60 C1 55 00", "8B 81 ?? ?? ?? ?? 8B B9 ?? ?? ?? ?? 8B 8E", "8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 8B 4E ?? 52 E8 ?? ?? ?? ?? 8B 46 ?? 8B 88 ?? ?? ?? ?? 51 E8 ?? ?? ?? ?? 83 C4 ?? B8 ?? ?? ?? ?? 5E 81 C4 ?? ?? ?? ?? C2 ?? ?? 90 90 90 90 90 90 81 EC", "8B 91 ?? ?? ?? ?? 8D 44 24 ?? 6A ?? 50 83 EA ?? 6A ?? 52 EB ?? A1 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 8B 4E ?? 52 E8 ?? ?? ?? ?? 8B 46 ?? 8B 88 ?? ?? ?? ?? 51 E8 ?? ?? ?? ?? 83 C4 ?? B8 ?? ?? ?? ?? 5E 81 C4 ?? ?? ?? ?? C2 ?? ?? 90 90 90 90 90 90 81 EC", "8B 82 ?? ?? ?? ?? 99 2B C2 D1 F9", "8B 80 ?? ?? ?? ?? 89 6C 24", "8B 80 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 81 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 86 ?? ?? ?? ?? A1 ?? ?? ?? ?? 85 C0", "8B 8A ?? ?? ?? ?? 81 E9 ?? ?? ?? ?? 51 6A ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B 83 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 83 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 6C 24 ?? 8B 83 ?? ?? ?? ?? 81 CD ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 6C 24 ?? 8B B0 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 83 C6 ?? B9 ?? ?? ?? ?? 8D 7C 24 ?? F3 ?? 8B 8C 24 ?? ?? ?? ?? 85 C9 74 ?? D9 41 ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 41 ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 41 ?? EB ?? 8B 80 ?? ?? ?? ?? 83 C2 ?? 3B D0 7E ?? 2B D0 8B 8B ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 83 C2 ?? 50 52 E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 84 24 ?? ?? ?? ?? D8 A4 24 ?? ?? ?? ?? D9 5C 24 ?? D9 84 24 ?? ?? ?? ?? D8 A4 24 ?? ?? ?? ?? 8D 4C 24 ?? 8D 54 24 ?? 51 52 D9 5C 24 ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? DD D8 D9 05 ?? ?? ?? ?? D9 E0 89 4C 24 ?? 89 54 24 ?? D9 44 24 ?? D9 E0 D9 44 24 ?? D9 E0 D9 C0 D8 4C 24 ?? A1 ?? ?? ?? ?? 89 44 24 ?? D9 C2 D8 4C 24 ?? DE E9 D9 5C 24 ?? D9 C2 D8 4C 24 ?? D9 C9 D8 4C 24 ?? DE E9 D9 5C 24 ?? D8 4C 24 ?? D9 C9 D8 4C 24 ?? DE E9 D9 54 24 ?? D8 4C 24 ?? D9 44 24 ?? D8 4C 24 ?? DE E9 D9 5C 24 ?? D9 44 24 ?? D8 4C 24 ?? D9 44 24 ?? D8 4C 24 ?? DE E9 D9 5C 24 ?? D9 44 24 ?? D8 4C 24 ?? D9 44 24 ?? D8 4C 24 ?? DE E9 8D 44 24 ?? 8D 4C 24 ?? 50 51 D9 5C 24 ?? E8 ?? ?? ?? ?? 8D 54 24 ?? 8D 44 24 ?? 52 50 DD D8 E8 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 6A ?? 8D 54 24 ?? 51 52 DD D8 E8 ?? ?? ?? ?? 8B 44 24 ?? 8B 4C 24 ?? 8B 54 24 ?? 89 84 24 ?? ?? ?? ?? 8B 44 24 ?? 89 8C 24 ?? ?? ?? ?? 8B 4C 24 ?? 89 44 24 ?? 8B 44 24 ?? 89 94 24 ?? ?? ?? ?? 8B 54 24 ?? 89 44 24 ?? 89 4C 24 ?? 8B 4C 24 ?? 89 54 24 ?? 8B 54 24 ?? 8D 44 24 ?? 89 4C 24 ?? 50 89 54 24 ?? E8 ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? D9 44 24 ?? D8 64 24 ?? 83 C4 ?? D9 1C ?? E8 ?? ?? ?? ?? 8B 9B ?? ?? ?? ?? 8B 4B ?? 8B 53 ?? 51 52 E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? 90 90 90 A1", "8B 80 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 83", "8B 88 ?? ?? ?? ?? 2B CB", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 6A ?? 55 6A ?? 83 EA ?? 68 ?? ?? ?? ?? D1 F8 52 50 E8 ?? ?? ?? ?? 5E", "8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 8B 15", "8B 88 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 52 83 E9", "8B 91 ?? ?? ?? ?? 8B 8E", "8B 88 ?? ?? ?? ?? D1 F9 81 E9 ?? ?? ?? ?? 89 8B", "8B 91 ?? ?? ?? ?? 8B 89 ?? ?? ?? ?? 6A", "8B BF ?? ?? ?? ?? 51", "8B B9 ?? ?? ?? ?? 8B 89", "8B B9 ?? ?? ?? ?? 8B 8B", "8B BF ?? ?? ?? ?? 6A", "8B 80 ?? ?? ?? ?? 68 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 83 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 82 ?? ?? ?? ?? 89 8B ?? ?? ?? ?? 2D", "8B 81 ?? ?? ?? ?? 99 2B C2 D1 F8 2D ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 15", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 6A ?? 53 6A ?? 83 EA ?? 68 ?? ?? ?? ?? D1 F8 52 50 E8 ?? ?? ?? ?? 5E 5B", "DB 80 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? D9 1C ?? DB 80 ?? ?? ?? ?? 51 D8 25 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 8B 82 ?? ?? ?? ?? 8B 8C 86 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 8B 51 ?? 52 50 E9", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? D1 F8 81 EA ?? ?? ?? ?? 2D ?? ?? ?? ?? 52 50 51 8B 8E ?? ?? ?? ?? 53", "8B 88 ?? ?? ?? ?? 6A ?? D1 F9 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? 6A ?? 53", "8B 82 ?? ?? ?? ?? D1 F8 83 E8 ?? 50 E8 ?? ?? ?? ?? 5E", "8B 91 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 FA 52 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 6A", "8B 88 ?? ?? ?? ?? D1 F9 51", "8B 80 ?? ?? ?? ?? 6A ?? 99 2B C2 8B 53 ?? 6A ?? 68 ?? ?? ?? ?? D1 F8 6A ?? 68", "8B 80 ?? ?? ?? ?? 6A ?? 99 2B C2 8B 53 ?? 6A ?? 68 ?? ?? ?? ?? D1 F8 6A ?? 6A", "8B 82 ?? ?? ?? ?? 6A ?? 6A ?? 57", "8B 88 ?? ?? ?? ?? 6A ?? 6A ?? 57", "8B 82 ?? ?? ?? ?? 6A ?? 6A ?? 53", "8B 91 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 FA 52 8B CE E8 ?? ?? ?? ?? 89 BE", "8B 91 ?? ?? ?? ?? 68 ?? ?? ?? ?? D1 FA", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 6A ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 53 53 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 53 53 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 6A ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8B CF D1 FA 81 EA ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "8B 91 ?? ?? ?? ?? 8D 86 ?? ?? ?? ?? D1 FA 6A ?? 50 81 EA ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 8B CF E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8D 86 ?? ?? ?? ?? 6A ?? 50 68 ?? ?? ?? ?? E9", "DB 80 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? D9 1C ?? DB 80 ?? ?? ?? ?? 51 D8 25 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 8B 82 ?? ?? ?? ?? 8B 8C 86 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 8B 51 ?? 52 50 E8", "DB 82 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DC 25 ?? ?? ?? ?? E8 ?? ?? ?? ?? 0F BF ?? 50 8B CE E8 ?? ?? ?? ?? EB", "8B 88 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 41", "8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 8B 4E ?? 52 E8 ?? ?? ?? ?? 8B 46 ?? 8B 88 ?? ?? ?? ?? 51 E8 ?? ?? ?? ?? 83 C4 ?? B8 ?? ?? ?? ?? 5E 81 C4 ?? ?? ?? ?? C2 ?? ?? 90 90 90 90 90 90 90", "8B 91 ?? ?? ?? ?? 8D 44 24 ?? 6A ?? 50 83 EA ?? 6A ?? 52 EB ?? A1 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 8B 4E ?? 52 E8 ?? ?? ?? ?? 8B 46 ?? 8B 88 ?? ?? ?? ?? 51 E8 ?? ?? ?? ?? 83 C4 ?? B8 ?? ?? ?? ?? 5E 81 C4 ?? ?? ?? ?? C2 ?? ?? 90 90 90 90 90 90 90", "8B 82 ?? ?? ?? ?? 6A ?? D1 F8 68 ?? ?? ?? ?? 83 E8 ?? 6A ?? 50 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? 6A ?? 55 6A ?? 6A ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 6A ?? 8B 91 ?? ?? ?? ?? 8B CE D1 FA 83 EA ?? 52 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 3B C5 74 ?? 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 F9 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 6A", "8B 91 ?? ?? ?? ?? 8B CE D1 FA 83 EA ?? 52 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 3B C5 74 ?? 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 F9 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 6A", "8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 F9 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 6A", "8B 82 ?? ?? ?? ?? D1 F8 50", "8B 88 ?? ?? ?? ?? 6A ?? 2B CF", "8B 88 ?? ?? ?? ?? 6A ?? D1 F9 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? 6A ?? 55 6A ?? 6A ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 50 6A ?? 8B CE 8B 82 ?? ?? ?? ?? D1 F8 83 E8 ?? 50 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 86", "8B 82 ?? ?? ?? ?? D1 F8 83 E8 ?? 50 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 86", "8B 91 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 FA 52 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 44 24 ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 83 C4 ?? 8D 4C 24 ?? 8B 82 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 51 D1 F8 6A ?? 50 8B CE E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? DC C0 D8 AE ?? ?? ?? ?? D8 15 ?? ?? ?? ?? D9 96 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 74", "8B 82 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 51 D1 F8 6A ?? 50 8B CE E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? DC C0 D8 AE ?? ?? ?? ?? D8 15 ?? ?? ?? ?? D9 96 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 74", "8B 82 ?? ?? ?? ?? 6A ?? D1 F8 68 ?? ?? ?? ?? 83 E8 ?? 6A ?? 50 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? 6A ?? 53", "8B 91 ?? ?? ?? ?? 8B CE D1 FA 83 EA ?? 52 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 9E", "8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 F9 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 54 24 ?? 68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 83 C4 ?? 8D 44 24 ?? 8B 91 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 50 D1 FA 6A ?? 52 8B CE E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? DC C0 D8 AE", "8B 91 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 50 D1 FA 6A ?? 52 8B CE E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? DC C0 D8 AE", "8B B8 ?? ?? ?? ?? 8B 86", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? D1 F8 81 EA ?? ?? ?? ?? 2D ?? ?? ?? ?? 52 50 51 8B 8E ?? ?? ?? ?? 6A", "DB 81 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DC 25 ?? ?? ?? ?? E8 ?? ?? ?? ?? 0F BF ?? 52 8B CE E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 8B 86", "DB 81 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DC 25 ?? ?? ?? ?? E8 ?? ?? ?? ?? 0F BF ?? 52 8B CE E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE", "DB 82 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DC 25 ?? ?? ?? ?? E8 ?? ?? ?? ?? 0F BF ?? 50 8B CE E8 ?? ?? ?? ?? E9", "8B 90 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 8B 46 ?? 81 E9 ?? ?? ?? ?? D1 FA 81 EA ?? ?? ?? ?? 51 52", "8B 91 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 FA 52 8B CE E8 ?? ?? ?? ?? 8B 86", "8B 82 ?? ?? ?? ?? 6A ?? D1 F8 68 ?? ?? ?? ?? 83 E8 ?? 6A ?? 50 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? 6A ?? 55 6A ?? 6A ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 6A ?? 8B 91 ?? ?? ?? ?? 8B CE D1 FA 83 EA ?? 52 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 3B C5 74 ?? 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 F9 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8", "8B 91 ?? ?? ?? ?? 8B CE D1 FA 83 EA ?? 52 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 3B C5 74 ?? 8B 0D ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 F9 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8", "8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 F9 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 54 24 ?? 68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 83 C4 ?? 8D 44 24 ?? 8B 91 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 50 D1 FA 6A ?? 52 8B CE E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? DC C0 8B 8E", "8B 91 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 50 D1 FA 6A ?? 52 8B CE E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? DC C0 8B 8E", "8B 91 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 FA 52 8B CE E8 ?? ?? ?? ?? 5F", "8B 90 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 6A ?? 53 D1 F8", "8B 82 ?? ?? ?? ?? 55", "8B 91 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 FA 52 8B CE E8 ?? ?? ?? ?? 39 BE", "8B 91 ?? ?? ?? ?? 6A ?? 6A ?? 68 ?? ?? ?? ?? C1 FA", "8B 91 ?? ?? ?? ?? 53 6A", "8B 88 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 68", "8B 82 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 68", "8B 91 ?? ?? ?? ?? 6A ?? 6A ?? 68 ?? ?? ?? ?? D1 FA", "8B 88 ?? ?? ?? ?? 53 6A", "8B 91 ?? ?? ?? ?? 6A ?? D1 FA", "8B 88 ?? ?? ?? ?? 6A ?? D1 F9 68 ?? ?? ?? ?? 83 E9 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? 6A ?? 55 6A ?? 6A ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 50 6A ?? 8B CE 8B 82 ?? ?? ?? ?? D1 F8 83 E8 ?? 50 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 0D", "8B 80 ?? ?? ?? ?? 6A ?? 53 6A ?? 53", "8B 80 ?? ?? ?? ?? 6A ?? 55 6A ?? 55 52 99 2B C2 6A ?? D1 F8 50 E8 ?? ?? ?? ?? 8D BE", "8B 81 ?? ?? ?? ?? 99 2B C2 6A", "8B 80 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A", "8B 80 ?? ?? ?? ?? 6A ?? 55 6A ?? 55 52 99 2B C2 6A ?? D1 F8 50 E8 ?? ?? ?? ?? 39 AE", "8B 82 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A", "8B 91 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? D1 FA 52 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 44 24 ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 83 C4 ?? 8D 4C 24 ?? 8B 82 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? 6A ?? 51 D1 F8 6A ?? 50 8B CE E8 ?? ?? ?? ?? D9 84 24 ?? ?? ?? ?? DC C0 D8 AE ?? ?? ?? ?? D8 15 ?? ?? ?? ?? D9 96 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 0F 84", "8B 80 ?? ?? ?? ?? 52 99", "8B 82 ?? ?? ?? ?? D1 F8 83 E8 ?? 50 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 39 AE ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B 0D");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? DA B0", "D8 0D ?? ?? ?? ?? D9 1C ?? 68 ?? ?? ?? ??");

		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution9Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution10Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 11 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution11Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 12 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution12Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 13 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution13Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 14 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution14Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 15 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution15Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 16 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution16Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 17 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution17Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 18 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution18Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 19 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution19Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 20 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution20Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 21 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution21Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 22 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution22Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 23 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution23Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 24 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution24Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 25 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution25Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 26 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution26Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 27 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution27Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 28 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution28Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 29 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution29Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 30 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution30Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 31 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution31Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 32 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution32Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 33 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution33Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 34 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution34Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 35 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution35Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 36 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution36Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 37 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution37Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 38 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution38Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 39 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution39Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 40 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution40Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 41 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution41Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 42 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution42Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 43 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution43Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 44 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution44Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 45 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution45Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 46 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution46Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 47 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution47Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 48 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution48Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 49 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution49Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 50 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution50Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 51 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution51Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 52 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution52Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 53 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution53Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 54 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution54Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 55 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution55Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 56 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution56Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 57 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution57Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 58 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution58Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 59 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution59Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 60 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution60Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 61 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution61Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 62 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution62Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 63 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution63Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 64 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution64Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 65 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution65Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 66 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution66Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 67 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution67Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 68 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution68Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 69 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution69Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 70 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution70Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 71 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution71Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 72 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution72Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 73 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution73Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 74 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution74Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 75 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution75Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 76 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution76Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 77 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution77Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 78 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution78Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 79 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution79Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 80 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution80Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 81 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution81Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 82 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution82Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 83 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution83Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 84 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution84Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 85 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution85Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 86 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution86Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 87 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution87Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 88 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution88Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 89 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution89Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 90 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution90Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 91 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution91Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 92 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution92Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 93 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution93Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 94 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution94Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 95 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution95Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 96 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution96Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 97 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution97Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 98 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution98Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 99 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution99Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 100 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution100Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 101 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution101Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 102 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution102Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 103 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution103Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 104 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution104Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 105 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution105Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 106 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution106Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 107 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution107Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 108 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution108Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 109 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution109Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 110 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution110Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 111 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution111Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 112 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution112Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 113 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution113Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 114 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution114Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 115 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution115Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 116 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution116Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 117 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution117Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 118 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution118Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 119 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution119Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 120 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution120Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 121 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution121Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 122 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution122Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 123 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution123Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 124 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution124Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 125 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution125Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 126 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution126Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 127 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution127Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 128 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution128Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 129 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution129Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 130 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution130Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 131 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution131Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 132 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution132Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 133 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution133Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 134 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution134Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 135 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution135Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 136 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution136Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 137 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution137Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 138 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution138Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 139 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution139Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 140 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution140Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 141 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution141Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 142 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution142Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 143 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution143Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 144 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution144Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 145 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution145Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 146 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution146Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 147 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution147Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 148 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution148Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 149 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution149Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 150 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution150Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 151 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution151Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 152 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution152Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 153 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution153Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 154 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution154Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 155 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution155Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 156 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution156Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 157 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution157Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScansResult[RendererResolutionScan], "\x90\x90", 2);

			RendererResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[RendererResolutionScan] + 8, "\x90\x90\x90", 3);

			RendererResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution1Scan], "\x90\x90\x90\x90", 4);

			ViewportResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan], [](SafetyHookContext& ctx)
			{
				Memory::WriteRegister(ctx.ecx, 0, 15, iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 15, "\x90\x90\x90\x90", 4);

			ViewportResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 15, [](SafetyHookContext& ctx)
			{
				Memory::WriteRegister(ctx.edx, 0, 15, iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution2Scan], "\x90\x90\x90", 3);

			ViewportResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 7, "\x90\x90\x90", 3);

			ViewportResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution3Scan], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution4Scan], "\x90\x90\x90", 3);

			ViewportResolutionWidthInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution4Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution4Scan] + 12, "\x90\x90\x90", 3);

			ViewportResolutionHeightInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution4Scan] + 12, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionInstructions5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution5Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution6Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution6Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution7Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions7Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution7Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution8Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions8Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution8Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution9Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction9Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution9Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution10Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction10Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution10Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution11Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions11Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution11Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution12Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction12Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution12Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution13Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction13Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution13Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution13Scan] + 27, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction13Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution13Scan] + 27, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution14Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction14Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution14Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution14Scan] + 26, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction14Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution14Scan] + 26, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution15Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions15Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution15Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution16Scan] + 9, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction16Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution16Scan] + 9, [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution16Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction16Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution16Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution17Scan] + 24, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction17Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution17Scan] + 24, [](SafetyHookContext& ctx)
			{
				FPU::FIDIV(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution17Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction17Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution17Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution18Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction18Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution18Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution18Scan] + 33, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction18Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution18Scan] + 33, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution19Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction19Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution19Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution19Scan] + 26, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction19Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution19Scan] + 26, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution20Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions20Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution20Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution21Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction21Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution21Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution22Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction22Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution22Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution23Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction23Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution23Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution24Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction24Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution24Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution25Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction25Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution25Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution26Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction26Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution26Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution27Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction27Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution27Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution28Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction28Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution28Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution29Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction29Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution29Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution30Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction30Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution30Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution31Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction31Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution31Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution32Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions32Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution32Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution33Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction33Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution33Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution34Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction34Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution34Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution35Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction35Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution35Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution36Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction36Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution36Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution37Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction37Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution37Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution38Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction38Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution38Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution39Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction39Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution39Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution40Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction40Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution40Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution41Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction41Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution41Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution42Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction42Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution42Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution43Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction43Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution43Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution44Scan] + 9, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction44Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution44Scan] + 9, [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution44Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction44Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution44Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution45Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction45Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution45Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution45Scan] + 33, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction45Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution45Scan] + 33, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution46Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction46Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution46Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution47Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction47Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution47Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution48Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction48Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution48Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution49Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions49Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution49Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution50Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction50Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution50Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution51Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction51Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution51Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution51Scan] + 26, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction51Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution51Scan] + 26, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution52Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction52Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution52Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution52Scan] + 28, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction52Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution52Scan] + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution53Scan] + 15, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction53Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution53Scan] + 15, [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution53Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction53Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution53Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution54Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions54Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution54Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution55Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions55Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution55Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution56Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions56Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution56Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xD0) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0xD4) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution57Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions57Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution57Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution58Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction58Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution58Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution59Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction59Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution59Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution60Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction60Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution60Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution61Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction61Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution61Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution61Scan] + 32, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction61Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution61Scan] + 32, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution62Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction62Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution62Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution62Scan] + 28, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction62Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution62Scan] + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution63Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction63Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution63Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution64Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction64Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution64Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution64Scan] + 28, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction64Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution64Scan] + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution65Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction65Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution65Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution66Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions66Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution66Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution67Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction67Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution67Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution68Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction68Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution68Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution69Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction69Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution69Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution70Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction70Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution70Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution70Scan] + 26, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction70Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution70Scan] + 26, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution71Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions71Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution71Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution72Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction72Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution72Scan], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution73Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions73Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution73Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution74Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction74Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution74Scan], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution75Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction75Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution75Scan], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution76Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction76Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution76Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution76Scan] + 33, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction76Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution76Scan] + 33, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution77Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction77Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution77Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution77Scan] + 28, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction77Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution77Scan] + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution78Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions78Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution78Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution79Scan] + 15, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction79Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution79Scan] + 15, [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution79Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction79Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution79Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution80Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions80Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution80Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution81Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction81Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution81Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution82Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction82Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution82Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution83Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction83Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution83Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution84Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction84Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution84Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution85Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction85Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution85Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution86Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction86Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution86Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution87Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction87Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution87Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution88Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction88Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution88Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution89Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction89Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution89Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution90Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction90Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution90Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution91Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction91Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution91Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution92Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction92Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution92Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution93Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction93Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution93Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution94Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction94Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution94Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution95Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction95Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution95Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution96Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction96Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution96Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution97Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction97Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution97Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution98Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction98Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution98Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution99Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction99Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution99Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution100Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction100Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution100Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution101Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction101Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution101Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution102Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction102Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution102Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution103Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction103Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution103Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution104Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction104Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution104Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution105Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction105Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution105Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution106Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction106Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution106Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution107Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction107Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution107Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution108Scan] + 15, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction108Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution108Scan] + 15, [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution108Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction108Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution108Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution109Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction109Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution109Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution110Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions110Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution110Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution111Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction111Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution111Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution112Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction112Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution112Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution113Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction113Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution113Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution114Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction114Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution114Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution115Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction115Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution115Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution116Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction116Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution116Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution117Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction117Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution117Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution118Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction118Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution118Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution119Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction119Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution119Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution120Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction120Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution120Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution121Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction121Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution121Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution122Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction122Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution122Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution123Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction123Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution123Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution124Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction124Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution124Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution125Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction125Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution125Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution126Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction126Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution126Scan], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution127Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction127Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution127Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution128Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction128Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution128Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution129Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction129Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution129Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution130Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction130Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution130Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution131Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions131Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution131Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution132Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction132Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution132Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution133Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction133Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution133Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution134Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction134Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution134Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution135Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction135Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution135Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution136Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction136Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution136Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution137Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction137Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution137Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution138Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			ViewportResolutionInstructions138Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution138Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution139Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction139Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution139Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution140Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction140Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution140Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution141Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction141Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution141Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution142Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction142Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution142Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution143Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction143Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution143Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution144Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction144Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution144Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution145Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction145Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution145Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution146Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction146Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution146Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution147Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction147Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution147Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution148Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction148Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution148Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution149Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction149Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution149Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution150Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction150Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution150Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution151Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction151Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution151Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution152Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction152Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution152Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution153Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction153Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution153Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution154Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction154Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution154Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution155Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction155Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution155Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution156Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction156Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution156Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution157Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction157Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution157Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayCameraFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MenuCameraFOVScan] - (std::uint8_t*)exeModule);

			fNewGameplayCameraHFOV = (fOriginalCameraHFOV * fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[GameplayCameraFOVScan] + 4, fNewGameplayCameraHFOV);

			CameraVFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[GameplayCameraFOVScan] + 46, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[GameplayCameraFOVScan] + 44, "\x90\x90\x90\x90\x90\x90", 6);
			
			GameplayCameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayCameraFOVScan] + 44, [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayCameraVFOV = *reinterpret_cast<float*>(CameraVFOVAddress);

				fNewGameplayCameraVFOV = (fCurrentGameplayCameraVFOV * fAspectRatioScale) * fFOVFactor;

				FPU::FMUL(fNewGameplayCameraVFOV);
			});

			fNewMenuCameraHFOV = fOriginalCameraHFOV * fAspectRatioScale;

			Memory::Write(CameraFOVInstructionsScansResult[MenuCameraFOVScan] + 10, fNewMenuCameraHFOV);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[MenuCameraFOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			MenuCameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MenuCameraFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuCameraVFOV = *reinterpret_cast<float*>(CameraVFOVAddress);

				fNewMenuCameraVFOV = fCurrentMenuCameraVFOV * fAspectRatioScale;

				FPU::FMUL(fNewMenuCameraVFOV);
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