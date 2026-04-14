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
HMODULE dllModule = nullptr;
HMODULE dllModule2 = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "NinaAgentChroniclesWidescreenFix";
std::string sFixVersion = "1.7";
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
constexpr float fDefaultCameraHFOV = 1.5707963705062866f;
constexpr float fDefaultCameraVFOV = 1.3613568544387817f;

// Ini variables
bool bFixActive;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewGameplayHFOV;
float fNewGameplayVFOV;
float fNewMenuVFOV;

// Game detection
enum class Game
{
	NAC,
	Unknown
};

enum ResolutionsInstructionsIndices
{
	ResListUnlock,
	ResWidthHeight
};

enum CameraFOVInstructionsIndices
{
	Gameplay,
	Menu
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::NAC, {"Nina: Agent Chronicles", "lithtech.exe"}},
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
static SafetyHookMid GameplayFOVInstructionsHook{};
static SafetyHookMid MenuVFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::NAC && bFixActive == true)
	{
		dllModule2 = Memory::GetHandle("cshell.dll");

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(dllModule2, "0F 82 ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? 3D", 
		exeModule, "8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? FF 90");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is cshell.dll+{:x}", ResolutionInstructionsScansResult[ResListUnlock] - (std::uint8_t*)dllModule2);

			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResWidthHeight] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock], 6);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 17, 6);

			//Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 30, 6);

			Memory::PatchBytes(ResolutionInstructionsScansResult[ResListUnlock] + 41, "\xEB");

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.eax + 0x3C);

				int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x40);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "8B 83 ?? ?? ?? ?? 8B 8B ?? ?? ?? ?? 8B 53", "D9 44 24 ?? D8 0D ?? ?? ?? ?? 51 8B 87");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Gameplay] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Menu] - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Gameplay], 12);

			GameplayFOVInstructionsHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayHFOV = Memory::ReadMem(ctx.ebx + 0x1B4);

				float& fCurrentGameplayVFOV = Memory::ReadMem(ctx.ebx + 0x1B8);

				if (Maths::isClose(fCurrentGameplayHFOV, fDefaultCameraHFOV) && Maths::isClose(fCurrentGameplayVFOV, fDefaultCameraVFOV))
				{
					fNewGameplayHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentGameplayHFOV, fAspectRatioScale, fFOVFactor); // Hipfire HFOV
				}
				else
				{
					fNewGameplayHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentGameplayHFOV, fAspectRatioScale); // All the other HFOVs during gameplay
				}

				if (Maths::isClose(fCurrentGameplayHFOV, fDefaultCameraHFOV) && Maths::isClose(fCurrentGameplayVFOV, fDefaultCameraVFOV))
				{
					fNewGameplayVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentGameplayVFOV, fFOVFactor); // Hipfire VFOV
				}
				else
				{
					fNewGameplayVFOV = fCurrentGameplayVFOV; // All other gameplay VFOVs
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewGameplayHFOV);

				ctx.eax = std::bit_cast<uintptr_t>(fNewGameplayVFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Menu], 4);

			MenuVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Menu], [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuVFOV = Memory::ReadMem(ctx.esp + 0xC);

				fNewMenuVFOV = fCurrentMenuVFOV / fAspectRatioScale;

				FPU::FLD(fNewMenuVFOV);
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
