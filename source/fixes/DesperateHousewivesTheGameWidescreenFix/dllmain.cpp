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
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "DesperateHousewivesTheGameWidescreenFix";
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
float fNewCameraFOV1;
float fNewCameraFOV2;
uint8_t* CameraFOV2ValueAddress;

// Game detection
enum class Game
{
	DHTG,
	Unknown
};

enum ResolutionListsIndices
{
	ResList1Scan,
	ResList2Scan
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DHTG, {"Desperate Housewives: The Game", "DesperateHousewives.exe"}},
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

static SafetyHookMid FamilySelectionAspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};

void WidescreenFix()
{
	if (eGameType == Game::DHTG && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
		
		std::vector<std::uint8_t*> ResolutionListsScansResult = Memory::PatternScan(exeModule, "C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B FF 04",
		"C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B FF 94");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResList1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResList2Scan] - (std::uint8_t*)exeModule);

			// Resolution List 1
			// 640x480
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 2, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 8, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 23, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 29, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 44, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 50, iCurrentResY);

			// 1152x864
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 65, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 71, iCurrentResY);

			// 1290x960
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 86, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 92, iCurrentResY);
			
			// 1280x1024
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 107, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 113, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 128, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 134, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 149, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 155, iCurrentResY);

			// 960x720
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 170, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 176, iCurrentResY);

			// 1200x900
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 191, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 197, iCurrentResY);

			// 1067x800
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 212, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 218, iCurrentResY);

			// Resolution List 2
			// 640x480
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 2, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 8, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 23, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 29, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 44, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 50, iCurrentResY);

			// 1152x864
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 65, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 71, iCurrentResY);

			// 1290x960
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 86, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 92, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 107, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 113, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 128, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 134, iCurrentResY);

			// 1280x768
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 149, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 155, iCurrentResY);

			// 1280x720
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 170, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 176, iCurrentResY);

			// 1600x900
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 191, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 197, iCurrentResY);

			// 1280x800
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 212, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 218, iCurrentResY);
		}

		std::uint8_t* FamilySelectionAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 58 ?? D9 E8 D9 58 ?? D9 05");
		if (FamilySelectionAspectRatioInstructionScanResult)
		{
			spdlog::info("Family Selection Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), FamilySelectionAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(FamilySelectionAspectRatioInstructionScanResult, 6);

			FamilySelectionAspectRatioInstructionHook = safetyhook::create_mid(FamilySelectionAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate family selection aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 03 D9 1C ?? 50", "A3 ?? ?? ?? ?? D9 05");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1Scan], 2);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.ebx);
				
				fNewCameraFOV1 = fCurrentCameraFOV1 * fFOVFactor;
				
				FPU::FLD(fNewCameraFOV1);
			});

			CameraFOV2ValueAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV2Scan] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2Scan], 5);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], [](SafetyHookContext& ctx)
			{
				const float& fCurrentCameraFOV2 = std::bit_cast<float>(ctx.eax);

				fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

				*reinterpret_cast<float*>(CameraFOV2ValueAddress) = fNewCameraFOV2;
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