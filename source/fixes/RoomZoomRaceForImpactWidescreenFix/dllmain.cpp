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

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "RoomZoomRaceForImpactWidescreenFix";
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

// Ini variables
bool bFixActive;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalGameplayFOV1 = 1.047196507f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewGameplayFOV1;
float fNewGameplayFOV2;
uintptr_t GameplayFOV2Offset;

enum ResolutionInstructionsIndices
{
	ResListUnlock,
	ResWidthHeight
};

enum CameraFOVInstructionsIndices
{
	Gameplay1,
	Gameplay2
};

// Game detection
enum class Game
{
	RZRFI,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RZRFI, {"Room Zoom: Race for Impact", "roomzoom.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(fFOVFactor);

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
static SafetyHookMid GameplayFOVInstruction2Hook{};

void SetARAndFOV()
{
	std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 89 ?? ?? ?? ?? 8B B1");
	if (AspectRatioInstructionScanResult)
	{
		spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

		Memory::PatchBytes(AspectRatioInstructionScanResult + 1, "\x0D");

		Memory::Write(AspectRatioInstructionScanResult + 2, &fNewAspectRatio);
	}
	else
	{
		spdlog::error("Failed to locate aspect ratio instruction memory address.");
		return;
	}

	std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? C1 E0 ?? 8D 88",
	"8B 0C 85 ?? ?? ?? ?? 51 E8 ?? ?? ?? ?? 83 3D");
	if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
	{
		spdlog::info("Gameplay FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Gameplay1] - (std::uint8_t*)exeModule);

		spdlog::info("Gameplay FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Gameplay2] - (std::uint8_t*)exeModule);

		fNewGameplayFOV1 = Maths::CalculateNewFOV_RadBased(fOriginalGameplayFOV1, fAspectRatioScale) * fFOVFactor;

		Memory::Write(CameraFOVInstructionsScansResult[Gameplay1] + 1, fNewGameplayFOV1);

		GameplayFOV2Offset = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[Gameplay2] + 3, Memory::PointerMode::Absolute);

		Memory::WriteNOPs(CameraFOVInstructionsScansResult[Gameplay2], 7);

		GameplayFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Gameplay2], [](SafetyHookContext& ctx)
		{
			float& fCurrentGameplayFOV2 = Memory::ReadMem(ctx.eax * 0x4 + GameplayFOV2Offset);

			fNewGameplayFOV2 = fCurrentGameplayFOV2 * fFOVFactor;

			ctx.ecx = std::bit_cast<uintptr_t>(fNewGameplayFOV2);
		});
	}
}

void WidescreenFix()
{
	if (eGameType == Game::RZRFI && bFixActive == true)
	{
		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "3B CA 0F 85 ?? ?? ?? ?? 3D",
		"8B 44 24 ?? 8B 4C 24 ?? 8B 7C 24 ?? 8B 54 24");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResListUnlock] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResWidthHeight] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock], 30);			

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& fCurrentWidth = Memory::ReadMem(ctx.esp + 0x14);

				int& fCurrentHeight = Memory::ReadMem(ctx.esp + 0x18);

				fNewAspectRatio = static_cast<float>(fCurrentWidth) / static_cast<float>(fCurrentHeight);

				SetARAndFOV();

				ResolutionInstructionsHook.disable();
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
