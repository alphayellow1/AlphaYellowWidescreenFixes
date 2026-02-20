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
std::string sFixName = "ArthurAndTheInvisiblesWidescreenFix";
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
	AATI,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Res1,
	Res2,
	Res3,
	Res4,
	Res5,
	Res6,
	Res7
};

enum AspectRatioInstructionsIndex
{
	CameraAR,
	HUD_AR1,
	HUD_AR2
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AATI, {"Arthur and the Invisibles", "GameModule.elb"}},
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

static SafetyHookMid ResolutionWidthInstruction1Hook{};
static SafetyHookMid ResolutionHeightInstruction1Hook{};
static SafetyHookMid ResolutionWidthInstruction2Hook{};
static SafetyHookMid ResolutionHeightInstruction2Hook{};
static SafetyHookMid ResolutionInstructions3Hook{};
static SafetyHookMid ResolutionInstructions4Hook{};
static SafetyHookMid ResolutionInstructions5Hook{};
static SafetyHookMid ResolutionWidthInstruction6Hook{};
static SafetyHookMid ResolutionHeightInstruction6Hook{};
static SafetyHookMid ResolutionInstructions7Hook{};
static SafetyHookMid CameraAspectRatioInstructionHook{};
static SafetyHookMid HUDAspectRatioInstruction1Hook{};
static SafetyHookMid HUDAspectRatioInstruction2Hook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::AATI && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0A 89 0D ?? ?? ?? ?? 8B 6A", "8B 08 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? C7 05",
		"8B 44 24 ?? 8B 4C 24 ?? A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 8B 94 24", "8B 44 24 ?? 8B 4C 24 ?? A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 8B 54 24", "DB 41 ?? 8B 51 ?? 89 54 24 ?? D8 3D",
		"74 ?? 8B 46 ?? 8B 0D ?? ?? ?? ?? 3B C1 74 ?? 0F AF C1 33 D2 F7 F7 89 46 ?? 8B 3D", "89 56 ?? 89 46 ?? 8B 0D ?? ?? ?? ?? 89 4D ?? 8B 15");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res5] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res6] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res7] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res1], 2);

			ResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res1] + 8, 3);

			ResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1] + 8, [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res2], 2);

			ResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res2] + 8, 3);

			ResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res3], 8);

			ResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res3], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res4], 8);

			ResolutionInstructions4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res5], 6);

			ResolutionInstructions5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res5], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Res6] + 1, "\x14");

			Memory::PatchBytes(ResolutionInstructionsScansResult[Res6] + 14, "\x07");

			Memory::PatchBytes(ResolutionInstructionsScansResult[Res6] + 34, "\x14");

			Memory::PatchBytes(ResolutionInstructionsScansResult[Res6] + 47, "\x07");

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res6] + 22, 3);

			ResolutionWidthInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res6] + 22, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res6] + 55, 3);

			ResolutionHeightInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res6] + 55, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res7], 6);

			ResolutionInstructions7Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res7], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});
		}		

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8B 15 ?? ?? ?? ?? 89 51 ?? EB", "D9 05 ?? ?? ?? ?? 8B 48 ?? 8D 44 24", "D8 0D ?? ?? ?? ?? 33 C0 66 8B 86");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CameraAR] - (std::uint8_t*)exeModule);
			
			spdlog::info("HUD Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUD_AR1] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUD_AR2] - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[CameraAR], 6);

			CameraAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CameraAR], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(fNewAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUD_AR1], 6);

			HUDAspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUD_AR1], [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUD_AR2], 6);

			HUDAspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUD_AR2], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewAspectRatio2);
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 41 ?? D8 0D");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x14);

				fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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
