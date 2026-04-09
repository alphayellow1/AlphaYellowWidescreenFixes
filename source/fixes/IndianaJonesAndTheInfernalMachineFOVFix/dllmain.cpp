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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "IndianaJonesAndTheInfernalMachineFOVFix";
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
constexpr float fOriginalPauseMenuFOV = 10.0f;
constexpr float fOriginalGameplayFOV = 90.0f;

// Ini variables
bool bFixActive;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
uint32_t iCurrentWidth;
uint32_t iCurrentHeight;
float fNewGeneralFOV;
float fNewGameplayFOV;
float fNewPauseMenuFOV;

// Game detection
enum class Game
{
	IJATIM,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	General,
	Gameplay,
	PauseMenu
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::IJATIM, {"Indiana Jones and the Infernal Machine", "Indy3D.exe"}},
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

static SafetyHookMid ResolutionWidthInstructionHook{};
static SafetyHookMid ResolutionHeightInstructionHook{};
static SafetyHookMid GeneralFOVInstructionHook{};
static SafetyHookMid PauseMenuFOVInstructionHook{};

void SetFOV()
{
	std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? D9 44 24 ?? EB ?? D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? 8B 44 24 ?? 50",
	"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 89 15", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? A3 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? ?? 75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8");
	if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
	{
		spdlog::info("General FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[General] - (std::uint8_t*)exeModule);

		spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Gameplay] - (std::uint8_t*)exeModule);

		spdlog::info("Pause Menu FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[2] - (std::uint8_t*)exeModule);

		Memory::WriteNOPs(CameraFOVInstructionsScansResult[General] + 4, 50);

		Memory::WriteNOPs(CameraFOVInstructionsScansResult[General], 4);

		GeneralFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[General], [](SafetyHookContext& ctx)
		{
			float& fCurrentGeneralFOV = Memory::ReadMem(ctx.esp + 0x8);

			fNewGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, fAspectRatioScale);

			FPU::FLD(fNewGeneralFOV);
		});

		fNewGameplayFOV = fOriginalGameplayFOV * fFOVFactor;

		Memory::Write(CameraFOVInstructionsScansResult[Gameplay] + 1, fNewGameplayFOV);

		fNewPauseMenuFOV = Maths::CalculateNewFOV_DegBased(fOriginalPauseMenuFOV, fAspectRatioScale);

		Memory::Write(CameraFOVInstructionsScansResult[PauseMenu] + 1, fNewPauseMenuFOV);
	}
}

void FOVFix()
{
	if (eGameType == Game::IJATIM && bFixActive == true)
	{
		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			ResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				iCurrentWidth = Memory::ReadRegister(ctx.eax);
			});

			ResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 23, [](SafetyHookContext& ctx)
			{
				iCurrentHeight = Memory::ReadRegister(ctx.eax);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				SetFOV();

				ResolutionWidthInstructionHook.reset();

				ResolutionHeightInstructionHook.reset();
			});
		}
		else
		{
			spdlog::error("Failed to find resolution instructions scan memory address.");
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