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
std::string sFixName = "Cricket2002WidescreenFix";
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
constexpr int iNewBitDepth = 32;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	CRICKET2002,
	Unknown
};

enum ResolutionInstructionsIndices
{
	MatchesRes,
	MainMenuRes1,
	MainMenuRes2
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CRICKET2002, {"Cricket 2002", "Cricket2002.exe"}},
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

static SafetyHookMid ResolutionInstructionsHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::CRICKET2002 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 44 24 ?? 8B 4C 24 ?? 8B 54 24 ?? A3 ?? ?? ?? ?? 8B 44 24 ?? 89 0D ?? ?? ?? ?? 50", "81 39 ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? 8B 49 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 75 ?? 8A 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 80 CA ?? 88 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 C4 ?? 8B 0C ?? 8B 94 06 ?? ?? ?? ?? C1 E1 ?? 03 CA 81 39 ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? 8B 49 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 75 ?? 8A 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 80 CA ?? 88 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 C4 ?? 8B 0C ?? 8B 94 06 ?? ?? ?? ?? C1 E1 ?? 03 CA 81 39 ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? 8B 49 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 75 ?? 8A 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 80 CA ?? 88 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 C4 ?? 8B 0C ?? 8B 94 06 ?? ?? ?? ?? C1 E1 ?? 03 CA 81 39 ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? 8B 49 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 75 ?? 8A 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 80 CA ?? 88 15 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 C4 ?? 8B 0C ?? 8B 94 06 ?? ?? ?? ?? C1 E1 ?? 03 CA 81 39 ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? 8B 49 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 75 ?? 8A 15", "81 39 ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? 8B 49 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 74 ?? 83 F9 ?? 75 ?? 53");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Matches Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[MatchesRes] - (std::uint8_t*)exeModule);

			spdlog::info("Main Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[MainMenuRes1] - (std::uint8_t*)exeModule);

			spdlog::info("Main Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[MainMenuRes2] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[MatchesRes], 12);

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[MatchesRes], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);

				ctx.edx = std::bit_cast<uintptr_t>(iNewBitDepth);
			});

			Memory::Write(ResolutionInstructionsScansResult[MainMenuRes1] + 2, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[MainMenuRes1] + 11, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[MainMenuRes2] + 2, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[MainMenuRes2] + 11, iCurrentResY);
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8D 54 24 ?? 51 52 E8 ?? ?? ?? ?? 6A");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionScanResult + 1, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 4C 24 ?? 68 ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 8D 54 24");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x4);

				fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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