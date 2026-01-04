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
std::string sFixName = "CrazyTaxiWidescreenFix";
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
float fNewHUDPlacement;
float fNewCameraFOV1;
double dNewCameraFOV2;
double dNewCameraFOV3;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	CT,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AR1Scan,
	AR2Scan
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CT, {"Crazy Taxi", "Crazy_Taxi_PC.exe"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};

void WidescreenFix()
{
	if (eGameType == Game::CT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "c7 05 ? ? ? ? ? ? ? ? c7 05 ? ? ? ? ? ? ? ? eb ? c7 05 ? ? ? ? ? ? ? ? c7 05 ? ? ? ? ? ? ? ? eb ? c7 05 ? ? ? ? ? ? ? ? c7 05 ? ? ? ? ? ? ? ? eb ? c7 05 ? ? ? ? ? ? ? ? c7 05 ? ? ? ? ? ? ? ? eb ? c7 05 ? ? ? ? ? ? ? ? c7 05 ? ? ? ? ? ? ? ? eb");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 320x240
			Memory::Write(ResolutionListScanResult + 6, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 16, iCurrentResY);

			// 640x480
			Memory::Write(ResolutionListScanResult + 28, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 38, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 50, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 60, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 72, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 82, iCurrentResY);

			// 1152x864
			Memory::Write(ResolutionListScanResult + 94, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 104, iCurrentResY);

			// 1280x960
			Memory::Write(ResolutionListScanResult + 116, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 126, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		std::uint8_t* HUDPlacementInstructionsScanResult = Memory::PatternScan(exeModule, "c7 44 24 ? ? ? ? ? c7 44 24 ? ? ? ? ? c7 44 24 ? ? ? ? ? c7 44 24 ? ? ? ? ? e8 ? ? ? ? a1 ? ? ? ? 8d 54 24");
		if (HUDPlacementInstructionsScanResult)
		{
			spdlog::info("HUD Placement Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), HUDPlacementInstructionsScanResult - (std::uint8_t*)exeModule);

			fNewHUDPlacement = sqrt(fNewAspectRatio / fOldAspectRatio);

			Memory::Write(HUDPlacementInstructionsScanResult + 4, fNewHUDPlacement);

			Memory::Write(HUDPlacementInstructionsScanResult + 28, fNewHUDPlacement);
		}
		else
		{
			spdlog::error("Failed to locate HUD placement instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "c7 05 ? ? ? ? ? ? ? ? e8 ? ? ? ? a1 ? ? ? ? 83 f8", "68 ? ? ? ? 68 ? ? ? ? e8 ? ? ? ? 6a ? e8 ? ? ? ? 66 c7 05");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2Scan] - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[AR1Scan] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2Scan] + 1, fNewAspectRatio);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "d9 05 ? ? ? ? dc 0d ? ? ? ? 8b 15", "dc 3d ? ? ? ? d9 1d ? ? ? ? d9 f2", "dc 3d ? ? ? ? d9 1d ? ? ? ? c3");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3Scan] - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV1Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(CameraFOVAddress);

				fNewCameraFOV1 = fCurrentCameraFOV1 * fAspectRatioScale * fFOVFactor;

				FPU::FLD(fNewCameraFOV1);
			});

			dNewCameraFOV2 = 64.0;

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);			

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FDIVR(dNewCameraFOV2);
			});

			dNewCameraFOV3 = 0.0001;

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);			

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FDIVR(dNewCameraFOV3);
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