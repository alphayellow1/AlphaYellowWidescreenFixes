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
std::string sFixName = "ProStrokeGolfWorldTour2007FOVFix";
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
int iNewCameraFOV;

// Game detection
enum class Game
{
	PSGWT2007,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AR1,
	AR2,
	AR3,
	AR4,
	AR5
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2,
	FOV3
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PSGWT2007, {"ProStroke Golf World Tour 2007", "Main.exe"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstructionsMidHook(uintptr_t CameraFOVAddress)
{
	int& iCurrentCameraFOV = *reinterpret_cast<int*>(CameraFOVAddress);

	iNewCameraFOV = (int)(iCurrentCameraFOV * fFOVFactor);

	FPU::FILD(iNewCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::PSGWT2007 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "C7 05 64 15 67 00 39 8E E3 3F 75 0A C7 05 64 15 67 00 AB AA AA 3F 8B 4C 24 44 5F 64 89 0D 00 00 00 00", "C7 05 64 15 67 00 39 8E E3 3F D9 5C 24 10 8B 4C 24 10 89 0D 70 15 67 00 D8 0D A0 06 60 00 D9 5C 24 14 8B 54 24 14 89 15 74 15 67 00 75 0A C7 05 64 15 67 00 AB AA AA 3F", "C7 80 C4 00 00 00 AB AA AA 3F C7 80 CC 00 00 00 00 00 00 48 C7 80 C0 00 00 00 A4 01 00 00 C7 80 D0 00 00 00 00 00 A0 43", "C7 87 C4 02 00 00 39 8E E3 3F EB 0A C7 87 C4 02 00 00 AB AA AA 3F D9 47 08 8B 17 D8 47 04 D9 57 04", "C7 86 B4 02 00 00 39 8E E3 3F EB 0A C7 86 B4 02 00 00 AB AA AA 3F 8B 8E 00 01 00 00 D9 45 10 8B 96 04 01 00 00");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1] - (std::uint8_t*)exeModule);
			
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR3] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR4] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR5] - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[AR1] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR1] + 18, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2] + 52, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR3] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR4] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR4] + 18, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR5] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR5] + 18, fNewAspectRatio);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "DB 87 C0 00 00 00 D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 87 CC 00 00 00", "DB 86 C0 00 00 00 D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2", "DB 80 C0 00 00 00 8B 88 C8 00 00 00 89 4C 24 18 D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 54 24 04 D9 5C 24 08 E8 ?? ?? ?? ?? 89 04 24 DB 04 24 D8 0D ?? ?? ?? ?? D9 44 24 04 D9 FF D9 C9 D9 C9 D8 7C 24 18 D9 44 24 08 D9 FE D9 54 24 08 DE C9 D8 74 24 18 D8 4C 24 14 DE F9 D8 7C 24 10 83 C4 0C C3");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.edi + 0xC0);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0xC0);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3], 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax + 0xC0);
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
