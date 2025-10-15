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
std::string sFixName = "SubCultureFOVFix";
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
float fNewCameraHFOV1;
float fNewCameraHFOV2;
float fNewCameraVFOV;

// Game detection
enum class Game
{
	SC,
	Unknown
};

enum ResolutionInstructionsIndex
{
	CameraHFOV1Scan,
	CameraVFOV1Scan,
	CameraHFOV2Scan,
	CameraVFOV2Scan,
	CameraHFOV3Scan,
	CameraVFOV3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SC, {"Sub Culture", "SCD3D.EXE"}},
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

static SafetyHookMid CameraHFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraHFOVInstruction3Hook{};
static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction3Hook{};

void CameraHFOVInstruction1MidHook(uintptr_t CameraHFOVAddress)
{
	float& fCurrentCameraHFOV1 = *reinterpret_cast<float*>(CameraHFOVAddress);

	fNewCameraHFOV1 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV1, 1.0f / fAspectRatioScale) / fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV1]
	}
}

void CameraHFOVInstruction2MidHook(uintptr_t CameraHFOVAddress)
{
	float& fCurrentCameraHFOV2 = *reinterpret_cast<float*>(CameraHFOVAddress);

	fNewCameraHFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV2, 1.0f / fAspectRatioScale) / fFOVFactor;

	_asm
	{
		fmul dword ptr ds:[fNewCameraHFOV2]
	}
}

void CameraVFOVInstructionMidHook(float fNewCameraHFOV)
{
	fNewCameraVFOV = fNewCameraHFOV * fNewAspectRatio;

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::SC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 46 ?? D9 E0 D8 0D ?? ?? ?? ?? D9 01", "D9 46 ?? D9 E0 D8 0D ?? ?? ?? ?? D9 C9", "D9 47 ?? D8 3D ?? ?? ?? ?? 8B 47", "D9 47 ?? D8 3D ?? ?? ?? ?? D9 47", "D8 4B ?? D9 C9 D9 5C 24", "D9 43 ?? D8 C9");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraVFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraVFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraHFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraVFOV3Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraHFOV1Scan], "\x90\x90\x90", 3);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraHFOV1Scan], [](SafetyHookContext& ctx) { CameraHFOVInstruction1MidHook(ctx.esi + 0x54); });

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraVFOV1Scan], "\x90\x90\x90", 3);

			CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraVFOV1Scan], [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(fNewCameraHFOV1); });
			
			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraHFOV2Scan], "\x90\x90\x90", 3);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraHFOV2Scan], [](SafetyHookContext& ctx) { CameraHFOVInstruction1MidHook(ctx.edi + 0x54); });

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraVFOV2Scan], "\x90\x90\x90", 3);

			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraVFOV2Scan], [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(fNewCameraHFOV1); });

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraHFOV3Scan], "\x90\x90\x90", 3);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraHFOV3Scan], [](SafetyHookContext& ctx) { CameraHFOVInstruction2MidHook(ctx.ebx + 0x54); });

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraVFOV3Scan], "\x90\x90\x90", 3);

			CameraVFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraVFOV3Scan], [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(fNewCameraHFOV2); });
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