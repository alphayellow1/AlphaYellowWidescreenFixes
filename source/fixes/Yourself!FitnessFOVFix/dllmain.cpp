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
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "Yourself!FitnessFOVFix";
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
double dFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
double dNewCameraFOV;
uintptr_t CameraFOV1Address;
uintptr_t CameraFOV2Address;

// Game detection
enum class Game
{
	YF,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1,
	Res2
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2,
	FOV3,
	FOV4
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::YF, {"Yourself!Fitness", "fitness_pc.exe"}},
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
	GetModuleFileNameW(dllModule, exePathW, MAX_PATH);
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", dFOVFactor);
	spdlog_confparse(dFOVFactor);

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
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, double fovFactor = 1.0)
{
	double& dCurrentCameraFOV = Memory::ReadMem(FOVAddress);

	dNewCameraFOV = Maths::CalculateNewFOV_RadBased(dCurrentCameraFOV, fAspectRatioScale, Maths::AngleMode::HalfAngle) * fovFactor;

	FPU::FLD(dNewCameraFOV);
}

void SetARAndFOV()
{
	std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 8B 54 24 ?? 89 0C");
	if (AspectRatioInstructionScanResult)
	{
		spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);		

		Memory::Write(AspectRatioInstructionScanResult + 2, &fNewAspectRatio2);
	}
	else
	{
		spdlog::error("Failed to locate aspect ratio instruction memory address.");
		return;
	}

	std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? 53", "DD 05 ?? ?? ?? ?? D9 F2",
	"DD 05 ?? ?? ?? ?? 8B 0D", "DD 05 ?? ?? ?? ?? A1");
	if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
	{
		spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

		CameraFOV1Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[FOV1] + 2, Memory::PointerMode::Absolute);

		CameraFOV2Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[FOV3] + 2, Memory::PointerMode::Absolute);

		Memory::WriteNOPs(CameraFOVInstructionsScansResult, FOV1, FOV4, 0, 6);

		CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
		{
			CameraFOVInstructionsMidHook(CameraFOV1Address);
		});

		CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
		{
			CameraFOVInstructionsMidHook(CameraFOV1Address);
		});

		CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], [](SafetyHookContext& ctx)
		{
			CameraFOVInstructionsMidHook(CameraFOV2Address);
		});

		CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4], [](SafetyHookContext& ctx)
		{
			CameraFOVInstructionsMidHook(CameraFOV1Address, dFOVFactor);
		});
	}
}

void FOVFix()
{
	if (eGameType == Game::YF && bFixActive == true)
	{		
		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 41 ?? 57 8D 79", "8B 44 24 ?? 8B 54 24 ?? 89 41 ?? 89 51");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2] - (std::uint8_t*)exeModule);

			ResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(0x0056E004);

				int& iCurrentHeight = Memory::ReadMem(0x0056E008);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				fNewAspectRatio2 = 0.75f / fAspectRatioScale;

				SetARAndFOV();

				ResolutionInstructions1Hook.disable();
			});

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);

				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				fNewAspectRatio2 = 0.75f / fAspectRatioScale;
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