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
std::string sFixName = "AgassiTennisGenerationFOVFix";
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
float fNewCameraFOV;

// Game detection
enum class Game
{
	ATG,
	Unknown
};

enum AspectRatioInstructionsIndex
{
	MenuARScan,
	GameplayARScan,
	CutscenesARScan,
};

enum CameraFOVInstructionsIndex
{
	MenuFOVScan,
	GameplayFOVScan,
	CutscenesFOVScan,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::ATG, {"Agassi Tennis Generation", "AGASSI.exe"}},
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

static SafetyHookMid MenuAspectRatioInstructionHook{};
static SafetyHookMid GameplayAspectRatioInstructionHook{};
static SafetyHookMid CutscenesAspectRatioInstructionHook{};
static SafetyHookMid MenuCameraFOVInstructionHook{};
static SafetyHookMid GameplayCameraFOVInstructionHook{};
static SafetyHookMid CutscenesCameraFOVInstructionHook{};

void AspectRatioInstructionsMidHook(SafetyHookContext& ctx)
{
	FPU::FDIV(fNewAspectRatio);
}

void CameraFOVInstructionsMidHook(uintptr_t SourceAddress, uintptr_t DestinationAddress, float fovFactor = 1.0f)
{
	const float& fCurrentCameraFOV = std::bit_cast<float>(SourceAddress);

	fNewCameraFOV = fCurrentCameraFOV * fAspectRatioScale * fovFactor;

	*reinterpret_cast<float*>(DestinationAddress) = fNewCameraFOV;
}

void FOVFix()
{
	if (eGameType == Game::ATG && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 B4 81 E8 00 00 00 D9 5C 24 08 8B 44 81 68 50 E8 8E BF 13 00 83 C4 10", 
		"D8 B4 86 E8 00 00 00 D9 5C 24 1C 8B 54 86 68 52 E8 2A 9A 13 00 83 C4 08", "D8 B4 85 E8 00 00 00 D9 5C 24 1C 8B 44 85 68 50 E8 65 71 13 00");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Menu Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[MenuARScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayARScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CutscenesARScan] - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(AspectRatioInstructionsScansResult[MenuARScan], 7);

			MenuAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[MenuARScan], AspectRatioInstructionsMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayARScan], 7);
			
			GameplayAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayARScan], AspectRatioInstructionsMidHook);
			
			Memory::WriteNOPs(AspectRatioInstructionsScansResult[CutscenesARScan], 7);

			CutscenesAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CutscenesARScan], AspectRatioInstructionsMidHook);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "89 94 81 A4 00 00 00 75 06 8B 81 98 00 00 00 D9 84 81 B8 00 00 00 8D 54 24 00 D8 8C 81 A4 00 00 00 52 D9 5C 24 04", 
		"89 94 8E A4 00 00 00 8B 86 98 00 00 00 D9 84 86 B8 00 00 00 8D 4C 24 14 D8 8C 86 A4 00 00 00 51 D9 5C 24 18 D9 84 86 B8 00 00 00 D8 8C 86 A4 00 00 00", "89 8C 85 ?? ?? ?? ?? 75");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Menu Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MenuFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesFOVScan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MenuFOVScan], 7);			
			
			MenuCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MenuFOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.edx, ctx.ecx + ctx.eax * 0x4 + 0xA4);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOVScan], 7);			

			GameplayCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.edx, ctx.esi + ctx.ecx * 0x4 + 0xA4, fFOVFactor);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOVScan], 7);
			
			CutscenesCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx, ctx.ebp + ctx.eax * 0x4 + 0xA4);
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