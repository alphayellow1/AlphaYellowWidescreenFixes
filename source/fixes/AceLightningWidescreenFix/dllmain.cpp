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
#include <locale>
#include <string>
#include <mutex>
#include <unordered_set>
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "AceLightningWidescreenFix";
std::string sFixVersion = "1.0";
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
float fNewCameraHFOV;
float fNewCameraFOV;

// Game detection
enum class Game
{
	AL,
	Unknown
};

enum ResolutionInstructionsIndex
{
	RendererResolutionScan,
	ViewportResolution1Scan,
	ViewportResolution2Scan,
	ViewportResolution3Scan
};

enum CameraFOVInstructionsIndex
{
	CameraHFOVScan,
	CameraVFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AL, {"Ace Lightning", "BBC - Ace Lightning.exe"}},
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

static SafetyHookMid RendererResolutionInstructionsHook{};
static SafetyHookMid ViewportResolutionWidthInstruction1Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction1Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};
static SafetyHookMid ViewportResolutionInstructions3Hook{};
static SafetyHookMid AspectRatioAndHUDInstructionHook{};
static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0x114);

	fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

	FPU::FMUL(fNewCameraFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::AL && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 0D", "8B 4E ?? 89 54 24 ?? 8B 56", "8B 4D ?? A1 ?? ?? ?? ?? 89 4C 24 ?? 8B 55 ??", "8B 70 ?? 8B 78");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution3Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScansResult[RendererResolutionScan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			RendererResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution1Scan], "\x90\x90\x90", 3);

			ViewportResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 7, "\x90\x90\x90", 3);

			ViewportResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution2Scan], "\x90\x90\x90", 3);

			ViewportResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 12, "\x90\x90\x90", 3);

			ViewportResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 12, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution3Scan], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		
		std::uint8_t* AspectRatioAndHUDInstructionScanResult = Memory::PatternScan(exeModule, "8B 08 D9 05 40 65 49 00 89 4E 68 8B 50 04");
		if (AspectRatioAndHUDInstructionScanResult)
		{
			spdlog::info("Aspect Ratio & HUD Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioAndHUDInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioAndHUDInstructionScanResult, "\x90\x90", 2);

			AspectRatioAndHUDInstructionHook = safetyhook::create_mid(AspectRatioAndHUDInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax);

				if (fCurrentCameraHFOV == 0.5f || fCurrentCameraHFOV == 0.129999995232f || fCurrentCameraHFOV == 0.800000011921f)
				{
					fNewCameraHFOV = fCurrentCameraHFOV;
				}
				else
				{
					fNewCameraHFOV = fCurrentCameraHFOV * fAspectRatioScale;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio & HUD instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "D8 88 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 80 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE C1 D9 98 ?? ?? ?? ?? D9 80", "D8 88 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 80 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE C1 D9 98 ?? ?? ?? ?? C3");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScanResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[CameraHFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[CameraVFOVScan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScanResult[CameraHFOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[CameraHFOVScan], CameraFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScanResult[CameraVFOVScan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[CameraVFOVScan], CameraFOVInstructionMidHook);
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