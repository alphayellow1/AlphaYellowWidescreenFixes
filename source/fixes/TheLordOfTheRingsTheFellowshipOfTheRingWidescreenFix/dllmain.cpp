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
std::string sFixVersion = "1.4";
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

// Variables
float fNewAspectRatio;
float fNewCameraFOV;
float fNewCutscenesCameraFOV2;
float fNewGameplayCameraFOV2;
float fViewDistanceFactor;
float fAspectRatioScale;
double dNewCutscenesCameraFOV1;
double dNewGameplayCameraFOV1;
float fNewViewDistance;
uint8_t* CutscenesCameraFOV1Address;
uint8_t* CutscenesCameraFOV2Address;
uint8_t* GameplayCameraFOV1Address;
uint8_t* GameplayCameraFOV2Address;

// Game detection
enum class Game
{
	TLOTRTFOTR,
	Unknown
};

enum CameraFOVInstrucionsIndex
{
	CutscenesFOV1Scan,
	CutscenesFOV2Scan,
	GameplayFOV1Scan,
	GameplayFOV2Scan
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

	while ((dllModule2 = GetModuleHandleA("Fellowship.rfl")) == nullptr)
	{
		spdlog::warn("Fellowship.rfl not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for Fellowship.rfl: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CutscenesCameraFOVInstruction1Hook{};
static SafetyHookMid CutscenesCameraFOVInstruction2Hook{};
static SafetyHookMid GameplayCameraFOVInstruction1Hook{};
static SafetyHookMid GameplayCameraFOVInstruction2Hook{};
static SafetyHookMid ViewDistanceInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::TLOTRTFOTR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 1D ?? ?? ?? ?? Df E0 F6 C4 ?? 7A ?? 81 FA");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FCOMP(fNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructionmemory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResults = Memory::PatternScan(exeModule, "DC 3D ?? ?? ?? ?? D9 99 48 02 00 00 FF 50 3C C2 04 00 90 90", "D9 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 E8 D9 F3 DC 0D ?? ?? ?? ??", "DC 3D ?? ?? ?? ?? D9 59 54 C2 04 00 90 90 90 90 90 90", "D9 05 ?? ?? ?? ?? D8 71 54 D9 E8 D9 F3 DC 0D ?? ?? ?? ?? C3 90");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResults) == true)
		{
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[CutscenesFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[CutscenesFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[GameplayFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResults[GameplayFOV2Scan] - (std::uint8_t*)exeModule);

			CutscenesCameraFOV1Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResults[CutscenesFOV1Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResults[CutscenesFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CutscenesCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[CutscenesFOV1Scan], [](SafetyHookContext& ctx)
			{
				double& dCurrentCutscenesCameraFOV1 = *reinterpret_cast<double*>(CutscenesCameraFOV1Address);

				dNewCutscenesCameraFOV1 = dCurrentCutscenesCameraFOV1 / (double)fAspectRatioScale;

				FPU::FDIVR(dNewCutscenesCameraFOV1);
			});

			CutscenesCameraFOV2Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResults[CutscenesFOV2Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResults[CutscenesFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CutscenesCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[CutscenesFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesCameraFOV2 = *reinterpret_cast<float*>(CutscenesCameraFOV2Address);

				fNewCutscenesCameraFOV2 = fCurrentCutscenesCameraFOV2 / fAspectRatioScale;

				FPU::FLD(fNewCutscenesCameraFOV2);
			});

			GameplayCameraFOV1Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResults[GameplayFOV1Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResults[GameplayFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			GameplayCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[GameplayFOV1Scan], [](SafetyHookContext& ctx)
			{
				double& dCurrentGameplayCameraFOV1 = *reinterpret_cast<double*>(GameplayCameraFOV1Address);

				dNewGameplayCameraFOV1 = (dCurrentGameplayCameraFOV1 / (double)fAspectRatioScale) / dFOVFactor;

				FPU::FDIVR(dNewGameplayCameraFOV1);
			});

			GameplayCameraFOV2Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResults[GameplayFOV2Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResults[GameplayFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			GameplayCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResults[GameplayFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayCameraFOV2 = *reinterpret_cast<float*>(GameplayCameraFOV2Address);

				fNewGameplayCameraFOV2 = fCurrentGameplayCameraFOV2 / fAspectRatioScale;

				FPU::FLD(fNewGameplayCameraFOV2);
			});		
		}

		std::uint8_t* ViewDistanceInstructionScanResult = Memory::PatternScan(dllModule2, "D9 80 A4 00 00 00 EB 06 D9 05 ?? ?? ?? ??");
		if (ViewDistanceInstructionScanResult)
		{
			spdlog::info("View Distance Instruction: Address is Fellowship.rfl+{:x}", ViewDistanceInstructionScanResult - (std::uint8_t*)dllModule2);			

			fNewViewDistance = 100.0f * fViewDistanceFactor;

			Memory::PatchBytes(ViewDistanceInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

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
