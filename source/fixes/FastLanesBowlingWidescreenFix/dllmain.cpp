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
std::string sFixName = "FastLanesBowlingWidescreenFix";
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
constexpr float fOriginalCameraFOV = 0.4f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio4;
float fNewCameraFOV;

// Game detection
enum class Game
{
	FLB_RESCONFIG,
	FLB_DX9,
	FLB_DX8,
	Unknown
};

enum AspectRatioInstructionsScansIndices
{
	AR1,
	AR2,
	AR3,
	AR4
};

enum CameraFOVInstructionsScansIndices
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	FOV5
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FLB_RESCONFIG, {"Fast Lanes Bowling (Resolution Configuration)", "reschange.exe"}},
	{Game::FLB_DX9, {"Fast Lanes Bowling (DX9)", "FastLanesDX9Test.exe"}},
	{Game::FLB_DX8, {"Fast Lanes Bowling (DX8)", "pcbowl.exe"}}
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

void SetARAndFOV()
{
	std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24",
	"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50",
	"C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 11");
	if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
	{
		spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1] - (std::uint8_t*)exeModule);

		spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2] - (std::uint8_t*)exeModule);

		spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR3] - (std::uint8_t*)exeModule);

		spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR4] - (std::uint8_t*)exeModule);

		Memory::Write(AspectRatioInstructionsScansResult, AR1, AR3, 1, fNewAspectRatio);

		fNewAspectRatio4 = 0.4f * fAspectRatioScale;

		Memory::Write(AspectRatioInstructionsScansResult[AR4] + 4, fNewAspectRatio4);
	}

	std::vector<std::uint8_t*> CameraFOVInstructionsScansResult;

	if (eGameType == Game::FLB_DX9)
	{
		CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24", "68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6",
		"68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50");
	}
	else if (eGameType == Game::FLB_DX8)
	{
		CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24", "68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6",
		"68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50", "68 ?? ?? ?? ?? 51 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B 15", "68 ?? ?? ?? ?? 51 50 E8 ?? ?? ?? ?? 83 C4 ?? 39 74 24");
	}

	if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
	{
		spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

		fNewCameraFOV = fOriginalCameraFOV * fAspectRatioScale;

		Memory::Write(CameraFOVInstructionsScansResult[FOV1] + 1, fNewCameraFOV);

		Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 1, fNewCameraFOV * fFOVFactor);

		Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 1, fNewCameraFOV);

		if (eGameType == Game::FLB_DX8)
		{
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5] - (std::uint8_t*)exeModule);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 1, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV5] + 1, fNewCameraFOV);
		}
	}
}

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::FLB_RESCONFIG)
		{
			std::uint8_t* ResolutionListUnlockScanResult = Memory::PatternScan(exeModule, "81 F9 ?? ?? ?? ?? 7C ?? 83 7C 24");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 40);
			}
			else
			{
				spdlog::error("Failed to find resolution list unlock scan memory address.");
				return;
			}
		}

		if (eGameType == Game::FLB_DX9 || eGameType == Game::FLB_DX8)
		{
			std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB");
			if (ResolutionInstructionsScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

				ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
				{
					const int& iCurrentWidth = Memory::ReadRegister(ctx.ecx);

					const int& iCurrentHeight = Memory::ReadRegister(ctx.edx);

					fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

					fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

					SetARAndFOV();

					ResolutionInstructionsHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to find resolution instructions scan memory address.");
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