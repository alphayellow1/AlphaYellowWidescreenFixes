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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "TheLordOfTheRingsTheFellowshipOfTheRingWidescreenFix";
std::string sFixVersion = "1.5";
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
constexpr float fOriginalCameraFOV = 64.0f;
constexpr double dOriginalCameraFOV = 64.0;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
double dFOVFactor;
float fViewDistanceFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
uintptr_t CameraFOV1Address;
uintptr_t CameraFOV2Address;
double dNewCutscenesCameraFOV1;
float fNewCutscenesCameraFOV2;
double dNewGameplayCameraFOV1;
float fNewGameplayCameraFOV2;
float fNewGameplayCameraFOV3;
float fNewViewDistance;

// Game detection
enum class Game
{
	TLOTRTFOTR,
	Unknown
};

enum CameraFOVInstrucionsIndex
{
	Cutscenes1,
	Cutscenes2,
	Gameplay1,
	Gameplay2,
	Gameplay3
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TLOTRTFOTR, {"The Lord of the Rings: The Fellowship of the Ring", "Fellowship.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", dFOVFactor);
	inipp::get_value(ini.sections["Settings"], "ViewDistanceFactor", fViewDistanceFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(dFOVFactor);
	spdlog_confparse(fViewDistanceFactor);

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
	bool bGameFound = false;

	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			bGameFound = true;
			break;
		}
	}

	if (bGameFound == false)
	{
		spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		return false;
	}

	dllModule2 = Memory::GetHandle("Fellowship.rfl");

	return true;
}

static SafetyHookMid CutscenesCameraFOVInstruction1Hook{};
static SafetyHookMid CutscenesCameraFOVInstruction2Hook{};
static SafetyHookMid GameplayCameraFOVInstruction1Hook{};
static SafetyHookMid GameplayCameraFOVInstruction2Hook{};
static SafetyHookMid GameplayCameraFOVInstruction3Hook{};
static SafetyHookMid ViewDistanceInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::TLOTRTFOTR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 7A ?? 81 FA");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = 16.0f;

			Memory::Write(AspectRatioInstructionScanResult + 2, &fNewAspectRatio2);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResults = Memory::PatternScan(exeModule, "DC 3D ?? ?? ?? ?? D9 99", "D9 05 ?? ?? ?? ?? D8 B1", "DC 3D ?? ?? ?? ?? D9 59",
		"D9 05 ?? ?? ?? ?? D8 71 ?? D9 E8", dllModule2, "8B 00 50 89 44 24 ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 6A");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResults) == true)
		{
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[Cutscenes1] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[Cutscenes2] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[Gameplay1] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[Gameplay2] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction 3: Address is Fellowship.rfl+{:x}", CameraFOVInstructionsScansResults[Gameplay3] - (std::uint8_t*)dllModule2);

			CameraFOV1Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResults[Cutscenes1] + 2, Memory::PointerMode::Absolute);

			CameraFOV2Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResults[Cutscenes2] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResults, Cutscenes1, Gameplay2, 0, 6);

			Memory::WriteNOPs(CameraFOVInstructionsScansResults[Gameplay3], 2);

			CutscenesCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[Cutscenes1], [](SafetyHookContext& ctx)
			{
				double& dCurrentCutscenesCameraFOV1 = Memory::ReadMem(CameraFOV1Address);

				dNewCutscenesCameraFOV1 = dCurrentCutscenesCameraFOV1 / (double)fAspectRatioScale;

				FPU::FDIVR(dNewCutscenesCameraFOV1);
			});			

			CutscenesCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[Cutscenes2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesCameraFOV2 = Memory::ReadMem(CameraFOV2Address);

				fNewCutscenesCameraFOV2 = fCurrentCutscenesCameraFOV2 / fAspectRatioScale;

				FPU::FLD(fNewCutscenesCameraFOV2);
			});

			GameplayCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[Gameplay1], [](SafetyHookContext& ctx)
			{
				double& dCurrentGameplayCameraFOV1 = Memory::ReadMem(CameraFOV1Address);

				dNewGameplayCameraFOV1 = (dCurrentGameplayCameraFOV1 / (double)fAspectRatioScale) / dFOVFactor;

				FPU::FDIVR(dNewGameplayCameraFOV1);
			});

			GameplayCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[Gameplay2], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayCameraFOV2 = Memory::ReadMem(CameraFOV2Address);

				fNewGameplayCameraFOV2 = fCurrentGameplayCameraFOV2 / fAspectRatioScale;

				FPU::FLD(fNewGameplayCameraFOV2);
			});

			GameplayCameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[Gameplay3], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayCameraFOV2 = Memory::ReadMem(ctx.eax);

				fNewGameplayCameraFOV3 = fCurrentGameplayCameraFOV2 * (float)dFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewGameplayCameraFOV3);
			});
		}

		std::uint8_t* ViewDistanceInstructionScanResult = Memory::PatternScan(dllModule2, "D9 80 ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? 85 C0");
		if (ViewDistanceInstructionScanResult)
		{
			spdlog::info("View Distance Instruction: Address is Fellowship.rfl+{:x}", ViewDistanceInstructionScanResult - (std::uint8_t*)dllModule2);			

			fNewViewDistance = 100.0f * fViewDistanceFactor;

			Memory::WriteNOPs(ViewDistanceInstructionScanResult, 6);

			ViewDistanceInstructionHook = safetyhook::create_mid(ViewDistanceInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewViewDistance);				
			});
		}
		else
		{
			spdlog::error("Failed to locate view distance instruction memory address.");
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
