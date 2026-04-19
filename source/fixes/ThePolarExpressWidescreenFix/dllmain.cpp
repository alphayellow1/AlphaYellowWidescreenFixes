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
std::string sFixName = "ThePolarExpressWidescreenFix";
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

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCutscenesAspectRatio;
float fNewGameplayAspectRatio;
float fNewCameraFOV;

// Game detection
enum class Game
{
	TPE_GAME,
	TPE_CONFIG,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	CutscenesAR,
	GameplayAR,
	PauseMenuAR
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TPE_CONFIG, {"The Polar Express - Configuration", "Config.exe"}},
	{Game::TPE_GAME, {"The Polar Express", "PolarExpress.exe"}}	
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
static SafetyHookMid CutscenesAspectRatioInstructionHook{};
static SafetyHookMid GameplayAspectRatioInstructionHook{};
static SafetyHookMid PauseMenuAspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::TPE_CONFIG)
		{
			std::uint8_t* ResolutionListUnlockScanResult = Memory::PatternScan(exeModule, "0F 8B ?? ?? ?? ?? 8B 55");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 6);

				Memory::WriteNOPs(ResolutionListUnlockScanResult + 28, 6);

				Memory::WriteNOPs(ResolutionListUnlockScanResult + 45, 2);

				Memory::PatchBytes(ResolutionListUnlockScanResult + 47, "\xEB");
			}
		}

		if (eGameType == Game::TPE_GAME)
		{			
			std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "8B 08 89 0D ?? ?? ?? ?? 8B 50");
			if (ResolutionInstructionsScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

				ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.eax);

					int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x4);

					fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

					fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

					ResolutionInstructionsHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}

			std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8B 50 ?? 8B 4C 24 ?? 83 C0 ?? 89 11 8B 40", 
			"D8 0D ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 83 C4 ?? 5E", "89 51 ?? 8B 8E ?? ?? ?? ?? 50");
			if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
			{
				spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CutscenesAR] - (std::uint8_t*)exeModule);

				spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAR] - (std::uint8_t*)exeModule);

				spdlog::info("Pause Menu Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[PauseMenuAR] - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[CutscenesAR], 3);

				CutscenesAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CutscenesAR], [](SafetyHookContext& ctx)
				{
					float& fCurrentCutscenesAspectRatio = Memory::ReadMem(ctx.eax + 0x68);

					fNewCutscenesAspectRatio = fCurrentCutscenesAspectRatio * fAspectRatioScale;

					ctx.edx = std::bit_cast<uintptr_t>(fNewCutscenesAspectRatio);
				});

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayAR], 6);				

				GameplayAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayAR], [](SafetyHookContext& ctx)
				{
					fNewGameplayAspectRatio = 0.75f / fAspectRatioScale;

					FPU::FMUL(fNewGameplayAspectRatio);
				});

				PauseMenuAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[PauseMenuAR], [](SafetyHookContext& ctx)
				{
					*reinterpret_cast<float*>(ctx.eax) = *reinterpret_cast<float*>(ctx.eax) * fAspectRatioScale;
				});
			}

			std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 8E");
			if (CameraFOVInstructionScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(CameraFOVInstructionScanResult, 4);

				CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x10);

					fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

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