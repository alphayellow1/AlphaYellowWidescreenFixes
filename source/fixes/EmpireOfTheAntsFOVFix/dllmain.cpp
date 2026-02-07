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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "EmpireOfTheAntsFOVFix";
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
double dFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
double dNewCameraFOV;
float fNewResX;
float fNewResY;
float fNewCameraFOV6;
float fNewCameraFOV7;

// Game detection
enum class Game
{
	EOTA,
	Unknown
};

enum AspectRatioInstructionsIndex
{
	AR1Scan,
	AR2Scan,
	AR3Scan,
	AR4Scan,
	AR5Scan
};

enum CameraFOVInstructionsIndex
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan,
	FOV4Scan,
	FOV5Scan,
	FOV6Scan,
	FOV7Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::EOTA, {"Empire of the Ants", "Game.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", dFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(dFOVFactor);

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

	dllModule2 = Memory::GetHandle("x3d.dll");

	return true;
}

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};
static SafetyHookMid AspectRatioInstruction5Hook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	FPU::FMUL(fNewResX);
	FPU::FLD(fNewResY);
}

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};
static SafetyHookMid CameraFOVInstruction7Hook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	FPU::FDIVR(dNewCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::EOTA && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(dllModule2, "D8 48 ?? D9 40 ?? D8 49 ?? 8D 4C 24", "D8 48 ?? D9 40 ?? D8 49 ?? C7 44 24", "D8 48 ?? D9 40 ?? D8 4E ?? 8D 44 24", "D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8B 46", "D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8D 44 24");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 49", "DC 3D ?? ?? ?? ?? D9 C0 D8 48", "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? 8D 44 24", "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8B 46", "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8D 44 24", "D9 46 ?? D8 0D ?? ?? ?? ?? D8 0D", "D9 44 24 ?? D9 F2");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instructions 1 Scan: Address is x3d.dll+{:x}", AspectRatioInstructionsScansResult[AR1Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Aspect Ratio Instructions 2 Scan: Address is x3d.dll+{:x}", AspectRatioInstructionsScansResult[AR2Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Aspect Ratio Instructions 3 Scan: Address is x3d.dll+{:x}", AspectRatioInstructionsScansResult[AR3Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Aspect Ratio Instructions 4 Scan: Address is x3d.dll+{:x}", AspectRatioInstructionsScansResult[AR4Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Aspect Ratio Instructions 5 Scan: Address is x3d.dll+{:x}", AspectRatioInstructionsScansResult[AR5Scan] - (std::uint8_t*)dllModule2);
			
			fNewResX = (float)iCurrentResX;

			fNewResY = (float)iCurrentResY;

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR1Scan], 6);			

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR1Scan], AspectRatioInstructionMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR2Scan], 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR2Scan], AspectRatioInstructionMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR3Scan], 6);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR3Scan], AspectRatioInstructionMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR4Scan], 6);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR4Scan], AspectRatioInstructionMidHook);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR5Scan], 6);

			AspectRatioInstruction5Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR5Scan], AspectRatioInstructionMidHook);
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1 Scan: Address is x3d.dll+{:x}", CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 2 Scan: Address is x3d.dll+{:x}", CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 3 Scan: Address is x3d.dll+{:x}", CameraFOVInstructionsScansResult[FOV3Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 4 Scan: Address is x3d.dll+{:x}", CameraFOVInstructionsScansResult[FOV4Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 5 Scan: Address is x3d.dll+{:x}", CameraFOVInstructionsScansResult[FOV5Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 6 Scan: Address is x3d.dll+{:x}", CameraFOVInstructionsScansResult[FOV6Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 7 Scan: Address is x3d.dll+{:x}", CameraFOVInstructionsScansResult[FOV7Scan] - (std::uint8_t*)dllModule2);

			dNewCameraFOV = (1.0 / (double)fAspectRatioScale) / dFOVFactor;

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1Scan], 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], CameraFOVInstructionMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2Scan], 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], CameraFOVInstructionMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3Scan], 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3Scan], CameraFOVInstructionMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV4Scan], 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4Scan], CameraFOVInstructionMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV5Scan], 6);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5Scan], CameraFOVInstructionMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6Scan], 3);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV6 = *reinterpret_cast<float*>(ctx.esi + 0x50);

				fNewCameraFOV6 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV6, 1.0f / fAspectRatioScale) / (float)dFOVFactor;

				FPU::FLD(fNewCameraFOV6);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV7Scan], 4);

			fNewCameraFOV7 = 5.0f;

			CameraFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV7Scan], [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewCameraFOV7);
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