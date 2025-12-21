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
std::string sFixName = "HighHeatMajorLeagueBaseball2003WidescreenFix";
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
float fNewCameraFOV;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	HHMLB2003,
	Unknown
};

enum ResolutionInstructionsIndices
{
	MainMenuResolutionScan,
	GameplayResolutionListScan,
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::HHMLB2003, {"High Heat Major League Baseball 2003", "HH2003.exe"}},
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

static SafetyHookMid ResolutionWidthInstruction4Hook{};
static SafetyHookMid ResolutionHeightInstruction4Hook{};
static SafetyHookMid ResolutionInstructions5Hook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (bFixActive == true && eGameType == Game::HHMLB2003)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "68 58 02 00 00 68 20 03 00 00 51 8B 0D 00 79 B4 00 52 56 50", "80 02 00 00 E0 01 00 00 80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 80 04 00 00 60 03 00 00 00 05 00 00 00 04 00 00 40 06 00 00 B0 04 00 00", "c7 05 ? ? ? ? ? ? ? ? c7 05 ? ? ? ? ? ? ? ? 89 0d ? ? ? ? 89 0d", "b8 ? ? ? ? a3 ? ? ? ? 8b 8e", "8b 0d ? ? ? ? 8b 15 ? ? ? ? a1 ? ? ? ? 68 ? ? ? ? 68");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[MainMenuResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Resolution List: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[GameplayResolutionListScan] - (std::uint8_t*)exeModule);

			// Main Menu Resolution (800x600)
			Memory::Write(ResolutionInstructionsScansResult[MainMenuResolutionScan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[MainMenuResolutionScan] + 1, iCurrentResY);

			// Gameplay Resolution List
			// 640x480
			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan], iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 4, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 8, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 12, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 16, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 20, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 24, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 28, iCurrentResY);

			// 1152x864
			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 32, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 36, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 40, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 44, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 48, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[GameplayResolutionListScan] + 52, iCurrentResY);

			// Resolution 3
			Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan] + 16, iCurrentResY);

			// Resolution 4
			Memory::Write(ResolutionInstructionsScansResult[Resolution4Scan] + 1, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution4Scan] + 64, iCurrentResY);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan] - 4, "\x90\x90", 2);

			ResolutionWidthInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan] - 4, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan] + 58, "\x90\x90\x90", 3);

			ResolutionHeightInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan] + 58, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			// Resolution 5
			ResolutionInstructions5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(0x00831CA0) = iCurrentResX;

				*reinterpret_cast<int*>(0x00831CA4) = iCurrentResY;
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? A1 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 3B C7");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

				fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction scan memory address.");
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