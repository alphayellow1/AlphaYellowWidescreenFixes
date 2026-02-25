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
std::string sFixName = "ShreksCarnivalCrazePartyGamesFOVFix";
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
float fNewCameraFOV;

// Game detection
enum class Game
{
	SCCPG,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AR1,
	AR2,
	AR3,
	AR4,
	AR5,
	AR6,
	AR7,
	AR8,
	AR9,
	AR10,
	AR11,
	AR12,
	AR13,
	AR14,
	AR15,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SCCPG, {"Shrek's Carnival Craze: Party Games", "ShrekCCPG.exe"}},
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

static SafetyHookMid CameraFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::SCCPG && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 1C 24 D9 04 24 59 C3 D9 05", "D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? 83 EC",
		"D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? 8D 4C 24 ?? D9 44 24 ?? D8 4C 24", "D9 05 ?? ?? ?? ?? C3 D9 05", "D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? 8B 44 24 ?? D9 1C 24 D9 04 24",
		"D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? 8B 44 24 ?? D9 1C 24 D9 00", "D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? 74 ?? D9 54 24 ?? EB ?? D9 C9 D9 54 24 ?? D9 C9 D9 44 24 ?? DD 05 ?? ?? ?? ?? DC C9 D9 C9 D9 5C 24 ?? D9 44 24 ?? D8 8E",
		"D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? D9 86 ?? ?? ?? ?? D9 44 24 ?? D8 CA", "D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? D9 86 ?? ?? ?? ?? D9 44 24 ?? DE CA",
		"D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? 8D 4C 24 ?? D9 44 24 ?? 51", "D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? 8B 84 24", "D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? 51",
		"D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? 74 ?? D9 54 24 ?? EB ?? D9 C9 D9 54 24 ?? D9 C9 D9 44 24 ?? DD 05 ?? ?? ?? ?? DC C9 D9 C9 D9 5C 24 ?? D9 44 24 ?? D8 4E", "D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? D9 46 ?? D9 44 24 ?? D8 CA",
		"D9 05 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D9 5C 24 ?? D9 46 ?? D9 44 24 ?? DE CA");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1] + 14 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR3] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR3] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR4] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR4] + 7 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR5] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR5] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 11: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR6] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 12: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR6] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 13: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR7] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 14: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR7] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 15: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR8] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 16: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR8] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 17: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR9] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 18: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR9] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 19: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR10] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 20: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR10] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 21: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR11] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 22: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR11] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 23: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR12] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 24: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR12] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 25: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR13] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 26: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR13] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 27: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR14] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 28: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR14] + 8 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 29: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR15] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 30: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR15] + 8 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[AR1] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR1] + 16, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR3] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR3] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR4] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR4] + 9, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR5] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR5] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR6] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR6] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR7] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR7] + 8, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR8] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR8] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR9] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR9] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR10] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR10] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR11] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR11] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR12] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR12] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR13] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR13] + 8, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR14] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR14] + 10, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR15] + 2, &fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR15] + 10, &fNewAspectRatio);
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 ?? B0 ?? D9 59 ?? 88 81 ?? ?? ?? ?? 88 81");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x4);

				fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

				FPU::FLD(fNewCameraFOV);
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