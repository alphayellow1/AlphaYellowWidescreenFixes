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
std::string sFixName = "MicroCommandosWidescreenFix";
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
float fNewCameraFOV;
float fNewCameraFOV2;

// Game detection
enum class Game
{
	MC,
	Unknown
};

enum AspectRatioInstructionsIndex
{
	AR1Scan,
	AR2Scan,
	AR3Scan,
	AR4Scan
};

enum CameraFOVInstructionsIndex
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan,
	FOV4Scan,
	FOV5Scan,
	FOV6Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MC, {"Micro Commandos", "Micro_Commandos.exe"}},
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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	FPU::FMUL(fNewAspectRatio);
}

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x9C);

	fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

	FPU::FLD(fNewCameraFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::MC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "BF 58 02 00 00 BE 20 03 00 00 57 56 6A 01 E8");

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8B 86 ?? ?? ?? ?? 51 8B 8E ?? ?? ?? ?? 52 50 51 8D 4E", "D8 8E ?? ?? ?? ?? D8 C9", "D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 51", "D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 52");

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 8E ?? ?? ?? ?? 52 50 51 8D 4E", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 15", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8D 54 24", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C", "D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 51", "D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 52");

		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScanResult + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScanResult + 1, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions scan memory address.");
			return;

			if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
			{
				spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR3Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR4Scan] - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR1Scan], 6);

				AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR1Scan], [](SafetyHookContext& ctx)
					{
						ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio);
					});

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR2Scan], 6);

				AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR2Scan], AspectRatioInstructionMidHook);

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR3Scan], 6);

				AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR3Scan], AspectRatioInstructionMidHook);

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR4Scan], 6);

				AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR4Scan], AspectRatioInstructionMidHook);
			}

			if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6Scan] - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1Scan], 6);

				CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], [](SafetyHookContext& ctx)
					{
						float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x9C);

						fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

						ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV2);
					});

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2Scan], 6);

				CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], CameraFOVInstructionMidHook);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3Scan], 6);

				CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3Scan], CameraFOVInstructionMidHook);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV4Scan], 6);

				CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4Scan], CameraFOVInstructionMidHook);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV5Scan], 6);

				CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5Scan], CameraFOVInstructionMidHook);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6Scan], 6);

				CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6Scan], CameraFOVInstructionMidHook);
			}
		}
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