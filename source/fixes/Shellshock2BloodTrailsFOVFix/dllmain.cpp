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
std::string sFixName = "Shellshock2BloodTrailsFOVFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewMainMenuFOV;
float fNewCutscenesFOV;
float fNewHipfireFOV;
float fNewZoomFOV;
uint8_t* HipfireCameraFOVAddress;
bool bInitializationFailed = false;

// Game detection
enum class Game
{
	SS2BT,
	Unknown
};

enum CameraFOVInstructionsScansIndices
{
	MainMenuFOVScan,
	CutscenesFOVScan,
	HipfireFOVScan,
	ZoomFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SS2BT, {"Shellshock 2: Blood Trails", "Shellshock 2.exe"}},
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
		bInitializationFailed = true;
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
		bInitializationFailed = true;
		return;
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

static SafetyHookMid MainMenuCameraFOVInstructionHook{};
static SafetyHookMid CutscenesCameraFOVInstructionHook{};
static SafetyHookMid HipfireCameraFOVInstructionHook{};
static SafetyHookMid ZoomCameraFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::SS2BT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "F3 0F 10 86 FC 00 00 00 0F 5A C0 0F 5A C9 F2 0F 5E C1 66 0F 5A C0", "D9 86 B4 00 00 00 D9 1C 24", "F3 0F 11 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 05 ?? ?? ?? ?? E8 ?? ?? ?? ??", "F3 0F 10 58 5C EB 06 F3 0F 10 5C 24 1C");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Main Menu Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MainMenuFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HipfireFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Zoom Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[ZoomFOVScan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[MainMenuFOVScan], "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			MainMenuCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MainMenuFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentMainMenuFOV = *reinterpret_cast<float*>(ctx.esi + 0xFC);

				fNewMainMenuFOV = Maths::CalculateNewFOV_RadBased(fCurrentMainMenuFOV, fAspectRatioScale);

				ctx.xmm0.f32[0] = fNewMainMenuFOV;
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CutscenesFOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			CutscenesCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV = *reinterpret_cast<float*>(ctx.esi + 0xB4);

				fNewCutscenesFOV = Maths::CalculateNewFOV_RadBased(fCurrentCutscenesFOV, fAspectRatioScale);

				FPU::FLD(fNewCutscenesFOV);
			});

			HipfireCameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[HipfireFOVScan] + 4, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[HipfireFOVScan], "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			HipfireCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireFOV = ctx.xmm0.f32[0];

				fNewHipfireFOV = Maths::CalculateNewFOV_DegBased(fCurrentHipfireFOV, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(HipfireCameraFOVAddress) = fNewHipfireFOV;
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[ZoomFOVScan], "\x90\x90\x90\x90\x90", 5);

			ZoomCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ZoomFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentWeaponHipfireFOV = *reinterpret_cast<float*>(ctx.eax + 0x5C);

				fNewZoomFOV = Maths::CalculateNewFOV_DegBased(fCurrentWeaponHipfireFOV, fAspectRatioScale);

				ctx.xmm3.f32[0] = fNewZoomFOV;
			});
		}
	}
}

DWORD __stdcall Main(void*)
{
	Logging();
	if (bInitializationFailed)
	{
		return FALSE;
	}

	Configuration();
	if (bInitializationFailed)
	{
		return FALSE;
	}

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
		thisModule = hModule;
		{
			HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
			if (mainHandle)
			{
				SetThreadPriority(mainHandle, THREAD_PRIORITY_HIGHEST);
				CloseHandle(mainHandle);
			}
			break;
		}
	case DLL_PROCESS_DETACH:
		if (!bInitializationFailed)
		{
			spdlog::info("DLL has been successfully unloaded.");
			spdlog::shutdown();
			// Hooks go out of scope and clean up automatically
		}
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}