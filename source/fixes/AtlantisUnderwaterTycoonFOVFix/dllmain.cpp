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
std::string sFixName = "AtlantisUnderwaterTycoonFOVFix";
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
constexpr float fOriginalCameraFOV = 0.75f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewCullingValue;

// Game detection
enum class Game
{
	AUT,
	Unknown
};

enum AspectRatioAndFOVInstructionsIndex
{
	ARAndFOV1Scan,
	ARAndFOV2Scan,
	ARAndFOV3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AUT, {"Atlantis Underwater Tycoon", "ut.exe"}},
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

static SafetyHookMid CameraCullingInstructionHook{};

void CameraCullingInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fadd dword ptr ds:[fNewCullingValue]
	}
}

void FOVFix()
{
	if (eGameType == Game::AUT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraCullingInstructionScanResult = Memory::PatternScan(exeModule, "D8 05 ?? ?? ?? ?? D9 5C 24 ?? 75");
		if (CameraCullingInstructionScanResult)
		{
			spdlog::info("Camera Culling Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraCullingInstructionScanResult - (std::uint8_t*)exeModule);

			fNewCullingValue = 16.0f;

			Memory::PatchBytes(CameraCullingInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraCullingInstructionHook = safetyhook::create_mid(CameraCullingInstructionScanResult, CameraCullingInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera culling instruction memory address.");
			return;
		}

		

		std::vector<std::uint8_t*> AspectRatioAndCameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 4E", "68 ?? ?? ?? ?? 8D 54 24 ?? 68 ?? ?? ?? ?? 52 E8", "8B 54 24 10 51 55 8D 86 48 AF 02 00 52 68 00 00 40 3F");
		if (Memory::AreAllSignaturesValid(AspectRatioAndCameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio & Camera FOV Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio & Camera FOV Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio & Camera FOV Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV3Scan] - (std::uint8_t*)exeModule);

			fNewCameraFOV = fOriginalCameraFOV * fFOVFactor;

			Memory::Write(AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV1Scan] + 1, fNewAspectRatio);

			Memory::Write(AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV1Scan] + 6, fNewCameraFOV);

			Memory::Write(AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV2Scan] + 1, fNewAspectRatio);

			Memory::Write(AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV2Scan] + 10, fNewCameraFOV);

			Memory::PatchBytes(AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV3Scan], "\x90\x90\x90\x90", 4);

			static SafetyHookMid AspectRatioInstruction3MidHook{};

			AspectRatioInstruction3MidHook = safetyhook::create_mid(AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV3Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});			

			Memory::Write(AspectRatioAndCameraFOVInstructionsScansResult[ARAndFOV3Scan] + 14, fNewCameraFOV);
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