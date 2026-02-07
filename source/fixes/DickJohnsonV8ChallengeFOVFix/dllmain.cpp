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
std::string sFixName = "DickJohnsonV8ChallengeFOVFix";
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
float fNewAspectRatio2;
float fNewCameraFOV;

// Game detection
enum class Game
{
	DJV8C,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	PauseMenuARScan,
	GameplayAR1Scan,
	GameplayAR2Scan
};

enum CameraFOVInstructionsIndices
{
	PauseMenuFOVScan,
	GameplayFOV1Scan,
	GameplayFOV2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DJV8C, {"Dick Johnson V8 Challenge", "Dick Johnson V8 Challenge.exe"}},
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

static SafetyHookMid PauseMenuAspectRatioInstructionHook{};
static SafetyHookMid GameplayAspectRatioInstruction1Hook{};
static SafetyHookMid GameplayAspectRatioInstruction2Hook{};
static SafetyHookMid PauseMenuCameraFOVHook{};
static SafetyHookMid GameplayCameraFOV1Hook{};
static SafetyHookMid GameplayCameraFOV2Hook{};

void AspectRatioInstructionsMidHook(SafetyHookContext& ctx)
{
	FPU::FMUL(fNewAspectRatio2);
}

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, uintptr_t& DestInstruction, float fovFactor = 1.0f)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	fNewCameraFOV = (fCurrentCameraFOV / fAspectRatioScale) / fovFactor;

	DestInstruction = std::bit_cast<uintptr_t>(fNewCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::DJV8C && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;		

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 8B 11 68", "D8 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 50", "D8 0D ?? ?? ?? ?? 8B 52 ?? 52 8B 15 ?? ?? ?? ?? 52 8B 94 24 ?? ?? ?? ?? 51 D9 1C ?? 52 8B 94 24 ?? ?? ?? ?? 52 FF 90 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 52 ?? 8B 01 8B 52 ?? 52 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 51 8B 40 ?? 8B 11 D9 40 ?? D8 0D ?? ?? ?? ?? D9 1C ?? FF 52 ?? 8B 0D");

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 84 24 ?? ?? ?? ?? 50 FF 92 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 8B 11", "8B 84 24 ?? ?? ?? ?? 50 FF 92 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 6A",
			"8B 94 24 ?? ?? ?? ?? 52 FF 90 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 52 ?? 8B 01 8B 52 ?? 52 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 51 8B 40 ?? 8B 11 D9 40 ?? D8 0D ?? ?? ?? ?? D9 1C ?? FF 52 ?? 8B 0D");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Pause Menu Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[PauseMenuARScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAR1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayAR2Scan] - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[PauseMenuARScan], 6);

			PauseMenuAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[PauseMenuARScan], AspectRatioInstructionsMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayAR1Scan], 6);

			GameplayAspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayAR1Scan], AspectRatioInstructionsMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayAR2Scan], 6);

			GameplayAspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayAR2Scan], AspectRatioInstructionsMidHook);
		}

		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Pause Menu Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[PauseMenuFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayFOV2Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[PauseMenuFOVScan], 7);

			PauseMenuCameraFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[PauseMenuFOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0xB8, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV1Scan], 7);

			GameplayCameraFOV1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x208, ctx.eax, fFOVFactor);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV2Scan], 7);

			GameplayCameraFOV2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x208, ctx.edx, fFOVFactor);
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