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
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHUDPlacement;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
float fNewCameraFOV5;
float fNewCameraFOV6;
float fNewCameraFOV9;
uint8_t* CameraFOV9Address;

// Game detection
enum class Game
{
	CT,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AR1,
	AR2
};

enum CameraFOVInstructionsIndices
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
	FOV10,
	FOV11,
	FOV12
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

static SafetyHookMid CameraFOVInstruction9Hook{};

void WidescreenFix()
{
	if (eGameType == Game::CT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB");
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

		std::uint8_t* HUDPlacementInstructionsScanResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8D 54 24");
		if (HUDPlacementInstructionsScanResult)
		{
			spdlog::info("HUD Placement Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), HUDPlacementInstructionsScanResult - (std::uint8_t*)exeModule);

			fNewHUDPlacement = sqrt(fAspectRatioScale);

			Memory::Write(HUDPlacementInstructionsScanResult + 4, fNewHUDPlacement);

			Memory::Write(HUDPlacementInstructionsScanResult + 28, fNewHUDPlacement);
		}
		else
		{
			spdlog::error("Failed to locate HUD placement instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 F8",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 66 C7 05");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2] - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[AR1] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2] + 1, fNewAspectRatio);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4 ?? C3 90 90 90 90 90 90 90 90 90 83 EC",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C3 90 90 90 90 90 A1", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E9 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 0D",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 35 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8D 51", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 3B C5 0F 84", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B C8 55",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 3B C5 74", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 83 C4",
		"A1 ?? ?? ?? ?? 83 C4 ?? A3 ?? ?? ?? ?? E9", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 57 E8 ?? ?? ?? ?? 8B 0D",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 8B 11", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B 44 24 ?? 8B 4C 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV7] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV8] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV9] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV10] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 11: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV11] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 12: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV12] - (std::uint8_t*)exeModule);

			fNewCameraFOV1 = 43.0f * fAspectRatioScale;

			fNewCameraFOV2 = 60.0f * fAspectRatioScale * fFOVFactor;

			fNewCameraFOV3 = 70.0f * fAspectRatioScale;

			fNewCameraFOV4 = 40.0f * fAspectRatioScale;

			fNewCameraFOV5 = 46.0f * fAspectRatioScale;

			fNewCameraFOV6 = 60.0f * fAspectRatioScale;

			Memory::Write(CameraFOVInstructionsScansResult[FOV1] + 6, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 6, fNewCameraFOV6);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 6, fNewCameraFOV3);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 6, fNewCameraFOV6);

			Memory::Write(CameraFOVInstructionsScansResult[FOV5] + 6, fNewCameraFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[FOV6] + 6, fNewCameraFOV6);

			Memory::Write(CameraFOVInstructionsScansResult[FOV7] + 6, fNewCameraFOV4);

			Memory::Write(CameraFOVInstructionsScansResult[FOV8] + 6, fNewCameraFOV6);

			CameraFOV9Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV9] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV9], 5);

			CameraFOVInstruction9Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV9], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV9 = *reinterpret_cast<float*>(CameraFOV9Address);

				fNewCameraFOV9 = fCurrentCameraFOV9 * fAspectRatioScale;

				ctx.eax = std::bit_cast<std::uintptr_t>(fNewCameraFOV9);
			});

			Memory::Write(CameraFOVInstructionsScansResult[FOV10] + 6, fNewCameraFOV6);

			Memory::Write(CameraFOVInstructionsScansResult[FOV11] + 6, fNewCameraFOV6);

			Memory::Write(CameraFOVInstructionsScansResult[FOV12] + 6, fNewCameraFOV5);
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