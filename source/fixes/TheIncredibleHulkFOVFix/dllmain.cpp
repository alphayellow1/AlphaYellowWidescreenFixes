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
std::string sFixName = "TheIncredibleHulkFOVFix";
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
constexpr float fOldAspectRatio = 16.0f / 9.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
double dFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
double dNewCameraFOV;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	TIH,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan,
	FOV4Scan,
	FOV5Scan,
	FOV6Scan,
	FOV7Scan,
	FOV8Scan,
	FOV9Scan,
	FOV10Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TIH, {"The Incredible Hulk", "Hulk.exe"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};
static SafetyHookMid CameraFOVInstruction7Hook{};
static SafetyHookMid CameraFOVInstruction8Hook{};
static SafetyHookMid CameraFOVInstruction9Hook{};
static SafetyHookMid CameraFOVInstruction10Hook{};

void CameraFOVInstructionsMidHook(SafetyHookContext& ctx)
{
	double& dCurrentCameraFOV = *reinterpret_cast<double*>(CameraFOVAddress);

	dNewCameraFOV = dCurrentCameraFOV * (double)fAspectRatioScale * dFOVFactor;

	FPU::FMUL(dNewCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::TIH && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "DC 0D ?? ?? ?? ?? 5E D9 5C 24", "DC 0D ?? ?? ?? ?? D9 5D ?? D9 45 ?? 5D C3", "DC 0D ?? ?? ?? ?? D9 5D ?? D9 45 ?? C9 C3 56", "DC 0D ?? ?? ?? ?? 5E D9 5D ?? D9 45 ?? D9 58 ?? 5D C3 55 8B EC 8B 45 ?? 56 8B 30 8B 06 8B 40 ?? 8B 00", "DC 0D ?? ?? ?? ?? 5E D9 5D ?? D9 45 ?? D9 58 ?? 5D C3 55 8B EC 8B 45 ?? 56 8B 30 8B 06 8B 40 ?? 8B 08", "DC 0D ?? ?? ?? ?? 5E D9 5D ?? D9 45 ?? D9 58 ?? 5D C3 55 8B EC 83 EC", "DC 0D ?? ?? ?? ?? 8B 06 8B 40", "DC 0D ?? ?? ?? ?? 8D 45 ?? 50 8D 45", "DC 0D ?? ?? ?? ?? 5E D9 5D ?? D9 45 ?? D9 58 ?? C9", "DC 0D ?? ?? ?? ?? D9 5D ?? D9 45 ?? DC C0");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV9Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV10Scan] - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV1Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV7Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV7Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV8Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction8Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV8Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV9Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction9Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV9Scan], CameraFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[FOV10Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction10Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV10Scan], CameraFOVInstructionsMidHook);
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