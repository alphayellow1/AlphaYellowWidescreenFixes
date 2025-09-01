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
std::string sFixVersion = "1.3";
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

static SafetyHookMid CutscenesCameraFOVInstruction1Hook{};

void CutscenesCameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	double& dCurrentCutscenesCameraFOV1 = *reinterpret_cast<double*>(CutscenesCameraFOV1Address);

	dNewCutscenesCameraFOV1 = dCurrentCutscenesCameraFOV1 / (double)fAspectRatioScale;

	_asm
	{
		fdivr qword ptr ds:[dNewCutscenesCameraFOV1]
	}
}

static SafetyHookMid CutscenesCameraFOVInstruction2Hook{};

void CutscenesCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCutscenesCameraFOV2 = *reinterpret_cast<float*>(CutscenesCameraFOV2Address);

	fNewCutscenesCameraFOV2 = fCurrentCutscenesCameraFOV2 / fAspectRatioScale;

	_asm
	{
		fld dword ptr ds:[fNewCutscenesCameraFOV2]
	}
}

static SafetyHookMid GameplayCameraFOVInstruction1Hook{};

void GameplayCameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	double& dCurrentGameplayCameraFOV1 = *reinterpret_cast<double*>(GameplayCameraFOV1Address);

	dNewGameplayCameraFOV1 = (dCurrentGameplayCameraFOV1 / (double)fAspectRatioScale) / dFOVFactor;

	_asm
	{
		fdivr qword ptr ds:[dNewGameplayCameraFOV1]
	}
}

static SafetyHookMid GameplayCameraFOVInstruction2Hook{};

void GameplayCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentGameplayCameraFOV2 = *reinterpret_cast<float*>(GameplayCameraFOV2Address);

	fNewGameplayCameraFOV2 = fCurrentGameplayCameraFOV2 / fAspectRatioScale;

	_asm
	{
		fld dword ptr ds:[fNewGameplayCameraFOV2]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::TLOTRTFOTR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioScanResult = Memory::PatternScan(exeModule, "00 00 C0 3F DB 0F 49 40");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScanResult - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScanResult, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio memory address.");
			return;
		}

		std::uint8_t* CutscenesCameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "DC 3D ?? ?? ?? ?? D9 99 48 02 00 00 FF 50 3C C2 04 00 90 90");
		if (CutscenesCameraFOVInstruction1ScanResult)
		{
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CutscenesCameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);
			
			CutscenesCameraFOV1Address = Memory::GetPointer<uint32_t>(CutscenesCameraFOVInstruction1ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CutscenesCameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			CutscenesCameraFOVInstruction1Hook = safetyhook::create_mid(CutscenesCameraFOVInstruction1ScanResult, CutscenesCameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate cutscenes camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CutscenesCameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 E8 D9 F3 DC 0D ?? ?? ?? ??");
		if (CutscenesCameraFOVInstruction2ScanResult)
		{
			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CutscenesCameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			CutscenesCameraFOV2Address = Memory::GetPointer<uint32_t>(CutscenesCameraFOVInstruction2ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CutscenesCameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CutscenesCameraFOVInstruction2Hook = safetyhook::create_mid(CutscenesCameraFOVInstruction2ScanResult, CutscenesCameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate cutscenes camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* GameplayCameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "DC 3D ?? ?? ?? ?? D9 59 54 C2 04 00 90 90 90 90 90 90");
		if (GameplayCameraFOVInstruction1ScanResult)
		{
			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			GameplayCameraFOV1Address = Memory::GetPointer<uint32_t>(GameplayCameraFOVInstruction1ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(GameplayCameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			GameplayCameraFOVInstruction1Hook = safetyhook::create_mid(GameplayCameraFOVInstruction1ScanResult, GameplayCameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate gameplay camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* GameplayCameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 71 54 D9 E8 D9 F3 DC 0D ?? ?? ?? ?? C3 90");
		if (GameplayCameraFOVInstruction2ScanResult)
		{
			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			GameplayCameraFOV2Address = Memory::GetPointer<uint32_t>(GameplayCameraFOVInstruction2ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(GameplayCameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			GameplayCameraFOVInstruction2Hook = safetyhook::create_mid(GameplayCameraFOVInstruction2ScanResult, GameplayCameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate gameplay camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* ViewDistanceInstructionScanResult = Memory::PatternScan(dllModule2, "D9 80 A4 00 00 00 EB 06 D9 05 ?? ?? ?? ??");
		if (ViewDistanceInstructionScanResult)
		{
			spdlog::info("View Distance Instruction: Address is Fellowship.rfl+{:x}", ViewDistanceInstructionScanResult - (std::uint8_t*)dllModule2);
			
			static SafetyHookMid ViewDistanceInstructionMidHook{};

			ViewDistanceInstructionMidHook = safetyhook::create_mid(ViewDistanceInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentViewDistance = *reinterpret_cast<float*>(ctx.eax + 0xA4);

				fCurrentViewDistance = 100.0f * fViewDistanceFactor;
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
