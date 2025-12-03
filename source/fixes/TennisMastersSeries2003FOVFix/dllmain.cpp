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
std::string sFixName = "TennisMastersSeries2003FOVFix";
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
	TMS2003,
	Unknown
};

enum CameraFOVInstructionsIndex
{
	Camera1To4FOVScan,
	Camera5and6FOVScan,
	Camera7FOVScan,
	CutscenesCameraFOV1Scan,
	CutscenesCameraFOV2Scan
};

enum AspectRatioInstructionsIndex
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan,
	AspectRatio4Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TMS2003, {"Tennis Masters Series 2003", "Tennis Masters Series 2003.exe"}},
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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};

void AspectRatioInstructionsMidHook(SafetyHookContext& ctx)
{
	FPU::FDIV(fNewAspectRatio);
}

static SafetyHookMid Camera1To4FOVInstructionHook{};
static SafetyHookMid Camera5and6FOVInstructionHook{};
static SafetyHookMid Camera7FOVInstructionHook{};
static SafetyHookMid CutscenesCameraFOVInstruction1Hook{};
static SafetyHookMid CutscenesCameraFOVInstruction2Hook{};

void CameraFOVInstructionMidHook(uintptr_t CameraFOVAddress, float fovFactor = 1.0f)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

	fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

	FPU::FLD(fNewCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::TMS2003 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 86 AC 00 00 00 D8 0D ?? ?? ?? ?? 8B 8E B4 00 00 00 8B 86 B0 00 00 00", "D9 84 8E C0 00 00 00 D8 0D ?? ?? ?? ?? 8B 4E 14 89 44 24 1C 89 54 24 18 8D 44 24 08", "D9 86 AC 00 00 00 D8 0D ?? ?? ?? ?? 8B 86 B4 00 00 00 D8 0D ?? ?? ?? ??", "D9 86 9C 00 00 00 D8 A6 98 00 00 00 8B 86 A0 00 00 00 DE C9 D8 86 98 00 00 00 D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2", "D9 84 86 C0 00 00 00 D8 0D ?? ?? ?? ?? 89 4C 24 24 8B 4E 14 89 54 24 28 8D 54 24 14 D8 0D ?? ?? ?? ?? 52 D9 F2");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera 1 to 4 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera1To4FOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera 5 & 6 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera5and6FOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera 7 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Camera7FOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesCameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesCameraFOV2Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[Camera1To4FOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			Camera1To4FOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Camera1To4FOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionMidHook(ctx.esi + 0xAC, fFOVFactor);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[Camera5and6FOVScan], "\x90\x90\x90\x90\x90\x90\x90", 7);

			Camera5and6FOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Camera5and6FOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionMidHook(ctx.esi + ctx.ecx * 0x4 + 0xC0, fFOVFactor);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[Camera7FOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			Camera7FOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Camera7FOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionMidHook(ctx.esi + 0xAC, fFOVFactor);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CutscenesCameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CutscenesCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesCameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionMidHook(ctx.esi + 0x9C);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CutscenesCameraFOV2Scan], "\x90\x90\x90\x90\x90\x90\x90", 7);

			CutscenesCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesCameraFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionMidHook(ctx.esi + ctx.eax * 0x4 + 0xC0);
			});
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 70 1C 8B 86 B0 00 00 00 D9 C1 D9 E0 D9 5C 24 04 D9 C1 D9 5C 24 08", "D8 70 1C D9 C1 D9 E0 D9 5C 24 04 D9 C1 D9 5C 24 08 DE C9 D9 54 24 0C D9 E0", "D8 71 1C 8B 8E 88 00 00 00 D9 C1 D9 E0 D9 5C 24 0C D9 C1 D9 5C 24 10 DE C9 D9 54 24 14 D9 E0 D9 5C 24 18 8B 50 5C 89 54 24 1C", "D8 B6 94 00 00 00 D9 C1 D9 E0 D9 5C 24 0C D9 C1 D9 5C 24 10 DE C9 D9 54 24 14 D9 E0");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio4Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio1Scan], "\x90\x90\x90", 3);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio1Scan], AspectRatioInstructionsMidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio2Scan], "\x90\x90\x90", 3);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio2Scan], AspectRatioInstructionsMidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio3Scan], "\x90\x90\x90", 3);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio3Scan], AspectRatioInstructionsMidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio4Scan], AspectRatioInstructionsMidHook);
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