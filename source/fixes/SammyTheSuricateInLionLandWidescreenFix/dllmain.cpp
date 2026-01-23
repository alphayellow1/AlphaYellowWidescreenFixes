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
#include <vector>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "SammyTheSuricateInLionLandWidescreenFix";
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
/*
int iHorizontalHUDMargin;
int iVerticalHUDMargin;
*/

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
uint8_t* CameraFOVAddress;
float fNewCameraFOV;
int iNewWidth3;

/*
int iNewHorizontalHUDMargin;
int iNewVerticalHUDMargin;
uint8_t* HorizontalHUDMarginAddress;
uint8_t* VerticalHUDMarginAddress;
*/

// Game detection
enum class Game
{
	STS,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1Scan,
	Res2Scan,
	Res3Scan,
	Res4Scan
};

enum HUDInstructionsIndices
{
	HUD1Scan,
	HUD2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::STS, {"Sammy the Suricate in Lion Land", "SamShare.exe"}},
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
	// inipp::get_value(ini.sections["Settings"], "HorizontalHUDMargin", iHorizontalHUDMargin);
	// inipp::get_value(ini.sections["Settings"], "VerticalHUDMargin", iVerticalHUDMargin);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	// spdlog_confparse(iHorizontalHUDMargin);
	// spdlog_confparse(iVerticalHUDMargin);

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

static SafetyHookMid ResolutionInstructions1Hook{};
static SafetyHookMid ResolutionInstructions2Hook{};
static SafetyHookMid ResolutionWidthInstruction4Hook{};
static SafetyHookMid ResolutionHeightInstruction4Hook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid HUDInstruction1Hook{};
static SafetyHookMid HUDInstruction2Hook{};

void WidescreenFix()
{
	if (eGameType == Game::STS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 3D", "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 0F 84", "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 2B CA 89 4C 24", "A3 ?? ?? ?? ?? 83 FD");
		
		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D9 E1");

		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res1Scan], 11);

			ResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			iNewWidth3 = 1024;

			static int* iNewWidth3Ptr = &iNewWidth3;

			Memory::Write(ResolutionInstructionsScansResult[Res3Scan] + 2, iNewWidth3Ptr);

			ResolutionWidthInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			ResolutionHeightInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4Scan] + 12, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(1.0f);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 E1 D9 15");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

				if (fCurrentCameraFOV != fNewCameraFOV)
				{
					fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
				}

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		/* Couldn't get this to work
		std::vector<std::uint8_t*> HUDInstructionsScansResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 8B 50 ?? C1 FA", "89 15 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 90");
		if (Memory::AreAllSignaturesValid(HUDInstructionsScansResult) == true)
		{
			spdlog::info("HUD Horizontal Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Vertical Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD2Scan] - (std::uint8_t*)exeModule);

			HorizontalHUDMarginAddress = Memory::GetPointerFromAddress<uint32_t>(HUDInstructionsScansResult[HUD1Scan] + 2, Memory::PointerMode::Absolute);

			VerticalHUDMarginAddress = Memory::GetPointerFromAddress<uint32_t>(HUDInstructionsScansResult[HUD2Scan] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(HUDInstructionsScansResult[HUD1Scan], 6);

			HUDInstruction1Hook = safetyhook::create_mid(HUDInstructionsScansResult[HUD1Scan], [](SafetyHookContext& ctx)
			{
				const int& iCurrentHorizontalHUDMargin = std::bit_cast<int>(ctx.ecx);

				if (iCurrentHorizontalHUDMargin != iNewHorizontalHUDMargin)
				{
					if (iCurrentHorizontalHUDMargin == 303)
					{
						iNewHorizontalHUDMargin = iCurrentHorizontalHUDMargin;
					}
					else if (iCurrentHorizontalHUDMargin < 79)
					{
						iNewHorizontalHUDMargin = iCurrentHorizontalHUDMargin + iHorizontalHUDMargin;
					}
					else if (iCurrentHorizontalHUDMargin > 600)
					{
						iNewHorizontalHUDMargin = iCurrentHorizontalHUDMargin + ((iCurrentResX - 800) - iHorizontalHUDMargin);
					}
					else
					{
						iNewHorizontalHUDMargin = iCurrentHorizontalHUDMargin + ((iCurrentResX - 800) / 2);
					}
				}

				*reinterpret_cast<int*>(HorizontalHUDMarginAddress) = iNewHorizontalHUDMargin;
			});

			Memory::WriteNOPs(HUDInstructionsScansResult[HUD2Scan], 6);

			HUDInstruction2Hook = safetyhook::create_mid(HUDInstructionsScansResult[HUD2Scan], [](SafetyHookContext& ctx)
			{
				const int& iCurrentVerticalHUDMargin = std::bit_cast<int>(ctx.edx);

				if (iCurrentVerticalHUDMargin == 195)
				{
					iNewVerticalHUDMargin = iCurrentVerticalHUDMargin;
				}
				else if (iCurrentVerticalHUDMargin < 59)
				{
					iNewVerticalHUDMargin = iCurrentVerticalHUDMargin + iVerticalHUDMargin;
				}
				else if (iCurrentVerticalHUDMargin > 400)
				{
					iNewVerticalHUDMargin = iCurrentVerticalHUDMargin + ((iCurrentResY - 600) - iVerticalHUDMargin);
				}
				else
				{
					iNewVerticalHUDMargin = iCurrentVerticalHUDMargin + ((iCurrentResY - 600) / 2);
				}

				*reinterpret_cast<int*>(VerticalHUDMarginAddress) = iNewVerticalHUDMargin;
			});
		}
		*/
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		thisModule = hModule;

		Logging();
		Configuration();
		if (DetectGame())
		{
			WidescreenFix();
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