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
std::string sFixName = "BarnyardWidescreenFix";
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
float fNewAspectRatio2;
float fNewCameraFOV;
float fNewResX;

// Game detection
enum class Game
{
	BARNYARD_GAME,
	Unknown
};

enum ResolutionListScans
{
	ResolutionList1Scan,
	ResolutionList2Scan,
	ResolutionList3Scan,
	ResolutionList4Scan,
	ResolutionList5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BARNYARD_GAME, {"Barnyard", "Barnyard.exe"}}
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

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::BARNYARD_GAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::vector<std::uint8_t*> ResolutionListsScansResult = Memory::PatternScan(exeModule, "C7 04 24 58 02 00 00 C7 44 24 04 20 03 00 00 C7 44 24 08 00 03 00 00 C7 44 24 10 60 03 00 00 C7 44 24 20 80 02 00 00 C7 44 24 24 E0 01 00 00 C7 44 24 28 20 03 00 00 C7 44 24 2C 58 02 00 00 C7 44 24 34 00 03 00 00 C7 44 24 3C 60 03 00 00 C7 44 24 48 40 06 00 00 C7 44 24 4C B0 04 00 00", "C7 46 24 20 03 00 00 C7 46 28 58 02 00 00", "20 03 00 00 58 02 00 00 20 00 00 00", "75 18 C7 44 24 14 20 03 00 00 C7 44 24 18 58 02 00 00", "85 F6 C7 44 24 14 20 03 00 00 C7 44 24 18 58 02 00 00");
			if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
			{
				spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResolutionList1Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResolutionList2Scan] - (std::uint8_t*)exeModule);

				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 11, iCurrentResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 3, iCurrentResY);

				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 51, iCurrentResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 59, iCurrentResY);

				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 3, iCurrentResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 10, iCurrentResY);

				Memory::Write(ResolutionListsScansResult[ResolutionList3Scan], iCurrentResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList3Scan] + 4, iCurrentResY);

				Memory::Write(ResolutionListsScansResult[ResolutionList4Scan] + 6, iCurrentResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList4Scan] + 14, iCurrentResY);

				Memory::Write(ResolutionListsScansResult[ResolutionList5Scan] + 6, iCurrentResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList5Scan] + 14, iCurrentResY);
			}

			std::uint8_t* HUDHorizontalResInstructionScanResult = Memory::PatternScan(exeModule, "68 00 00 16 44 68 00 00 48 44");
			if (HUDHorizontalResInstructionScanResult)
			{
				spdlog::info("HUD Horizontal Res Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDHorizontalResInstructionScanResult - (std::uint8_t*)exeModule);

				fNewResX = 800.0f * fAspectRatioScale;

				Memory::Write(HUDHorizontalResInstructionScanResult + 6, fNewResX);
			}
			else
			{
				spdlog::error("Failed to locate HUD horizontal resolution instruction scan memory address.");
				return;
			}

			std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 41 ?? 59 C2 ?? ?? CC");
			if (CameraFOVInstructionScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

				CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCameraFOV = std::bit_cast<float>(ctx.eax);

					fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

					*reinterpret_cast<float*>(ctx.ecx + 0x8) = fNewCameraFOV;
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction scan memory address.");
				return;
			}
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