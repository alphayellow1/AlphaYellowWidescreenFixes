// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>
#include <vector>
#include <algorithm>
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
std::string sFixName = "LotusChallengeFOVFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
float fNewCameraFOV5;
float fNewCutscenesFOV;

// Game detection
enum class Game
{
	LC,
	Unknown
};

enum CameraFOVInstructionIndex
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	MainMenuFOV1,
	MainMenuFOV2,
	CutscenesFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::LC, {"Lotus Challenge", "lotus.exe"}},
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

static SafetyHookMid CutscenesFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::LC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 C9 89 51");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionScanResult + 2, &fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "C7 40 ?? ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 89 54 24",
		"C7 43 ?? ?? ?? ?? ?? 8B 87", "C7 41 ?? ?? ?? ?? ?? 89 BE", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 57", "C7 41 ?? ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 0A",
		"C7 40 ?? ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 09", "89 43 ?? E9 ?? ?? ?? ?? 8B CD");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Main Menu Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MainMenuFOV1] - (std::uint8_t*)exeModule);

			spdlog::info(" Main Menu Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MainMenuFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesFOV] - (std::uint8_t*)exeModule);

			fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(1.570795059f, fAspectRatioScale);

			fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(1.047196746f, fAspectRatioScale);

			fNewCameraFOV3 = Maths::CalculateNewFOV_RadBased(1.221729517f, fAspectRatioScale);

			fNewCameraFOV4 = Maths::CalculateNewFOV_RadBased(1.658061504f, fAspectRatioScale);

			fNewCameraFOV5 = Maths::CalculateNewFOV_RadBased(0.5585049391f, fAspectRatioScale);

			Memory::Write(CameraFOVInstructionsScansResult[FOV1] + 3, fNewCameraFOV1 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 3, fNewCameraFOV2 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 3, fNewCameraFOV1 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 35, fNewCameraFOV2 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 81, fNewCameraFOV1 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 110, fNewCameraFOV2 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 1, fNewCameraFOV3 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 6, fNewCameraFOV2 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 36, fNewCameraFOV4 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 41, fNewCameraFOV1 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 68, fNewCameraFOV3 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 77, fNewCameraFOV4 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 95, fNewCameraFOV2 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 104, fNewCameraFOV1 * fFOVFactor);

			Memory::Write(CameraFOVInstructionsScansResult[MainMenuFOV1] + 3, fNewCameraFOV5);

			Memory::Write(CameraFOVInstructionsScansResult[MainMenuFOV2] + 3, fNewCameraFOV2);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV], 3);

			CutscenesFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				const float& fCurrentCutscenesFOV = Memory::ReadRegister(ctx.eax);

				fNewCutscenesFOV = Maths::CalculateNewFOV_RadBased(fCurrentCutscenesFOV, fAspectRatioScale);

				Memory::ReadMem(ctx.ebx + 0x4) = fNewCutscenesFOV;
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