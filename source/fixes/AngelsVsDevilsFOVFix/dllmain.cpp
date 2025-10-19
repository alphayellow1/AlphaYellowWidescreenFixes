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
#include <cmath>
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
std::string sFixName = "AngelsVsDevilsFOVFix";
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
float fNewCameraVFOV;

// Game detection
enum class Game
{
	AVD,
	Unknown
};

enum CameraHFOVInstructionsIndex
{
	CameraHFOV1Scan,
	CameraHFOV2Scan,
	CameraHFOV3Scan,
	CameraHFOV4Scan,
	CameraHFOV5Scan,
};

enum CameraVFOVInstructionsIndex
{
	CameraVFOV1Scan,
	CameraVFOV2Scan,
	CameraVFOV3Scan,
	CameraVFOV4Scan,
	CameraVFOV5Scan,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AVD, {"Angels vs Devils", "AngelsvsDevils.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);

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
static SafetyHookMid CameraHFOVInstruction4Hook{};
static SafetyHookMid CameraHFOVInstruction5Hook{};

void CameraHFOVInstructionMidHook(uintptr_t CameraHFOVAddress)
{
	float& fCurrentCameraHFOV = *reinterpret_cast<float*>(CameraHFOVAddress);

	if (fCurrentCameraHFOV == 0.7853981853f)
	{
		fNewCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, fAspectRatioScale, fFOVFactor);
	}
	else
	{
		fNewCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, fAspectRatioScale);
	}

	FPU::FLD(fNewCameraHFOV);
}

static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction3Hook{};
static SafetyHookMid CameraVFOVInstruction4Hook{};
static SafetyHookMid CameraVFOVInstruction5Hook{};

void CameraVFOVInstructionMidHook(uintptr_t CameraVFOVAddress)
{
	float& fCurrentCameraVFOV = *reinterpret_cast<float*>(CameraVFOVAddress);

	if (Maths::isClose(fCurrentCameraVFOV, 0.589048624f / fAspectRatioScale))
	{
		fNewCameraVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentCameraVFOV * fAspectRatioScale, fFOVFactor);
	}
	else
	{
		fNewCameraVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentCameraVFOV * fAspectRatioScale);
	}

	FPU::FLD(fNewCameraVFOV);
}

void FOVFix()
{
	if (eGameType == Game::AVD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraHFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 43 1C D8 0D ?? ?? ?? ?? D9 F2 DD D8", "D9 46 1C 8B 18 D8 0D ?? ?? ?? ?? 8B 68 04 89 5C 24 14 89 6C 24 18 8B 48 08", "D9 46 1C D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51 C7 44 24 54 00 00 00 00 8B 44 24 54 C7 44 24 5C 00 00 00 00 8B 54 24 5C 89 44 24 18 D9 1C 24 89 4C 24 1C 89 54 24 20 E8 ?? ?? ?? ?? 83 EC 08 8D 4C 24 68 8B C4", "D9 46 1C D8 0D ?? ?? ?? ?? 51 D9 1C 24 E8 ?? ?? ?? ?? 83 EC 08 8B C4 83 EC 0C 8B CC D9 10 D9 50 04 D9 58 08", "D9 46 1C D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51 C7 44 24 54 00 00 00 00 8B 44 24 54 C7 44 24 5C 00 00 00 00 8B 54 24 5C 89 44 24 18 D9 1C 24 89 4C 24 1C 89 54 24 20 E8 ?? ?? ?? ?? 83 EC 08 8D 4C 24 74 8B C4 6A 00 6A 00 D9 10 D9 50 04 D9 58 08");
		if (Memory::AreAllSignaturesValid(CameraHFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV5Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV1Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV1Scan], [](SafetyHookContext& ctx) { CameraHFOVInstructionMidHook(ctx.ebx + 0x1C); });

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV2Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV2Scan], [](SafetyHookContext& ctx) { CameraHFOVInstructionMidHook(ctx.esi + 0x1C); });

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV3Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV3Scan], [](SafetyHookContext& ctx) { CameraHFOVInstructionMidHook(ctx.esi + 0x1C); });

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV4Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV4Scan], [](SafetyHookContext& ctx) { CameraHFOVInstructionMidHook(ctx.esi + 0x1C); });

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV5Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraHFOVInstruction5Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV5Scan], [](SafetyHookContext& ctx) { CameraHFOVInstructionMidHook(ctx.esi + 0x1C); });
		}

		std::vector<std::uint8_t*> CameraVFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 5D 00 D9 43 20 D8 0D ?? ?? ?? ?? D9 F2 DD D8", "D9 9E C8 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? 51 D9 1C 24 E8 ?? ?? ?? ?? 83 EC 08", "D9 9E 98 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51", "D9 9E 98 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? 51 D9 1C 24 E8 ?? ?? ?? ?? 83 EC 08 8B C4", "D9 9E A8 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51 C7 44 24 54 00 00 00 00 8B 44 24 54");
		if (Memory::AreAllSignaturesValid(CameraVFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionsScansResult[CameraVFOV1Scan] + 3 - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionsScansResult[CameraVFOV2Scan] + 3 - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionsScansResult[CameraVFOV3Scan] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionsScansResult[CameraVFOV4Scan] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionsScansResult[CameraVFOV5Scan] + 6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionsScansResult[CameraVFOV1Scan] + 3, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraVFOVInstructionsScansResult[CameraVFOV1Scan] + 3, [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(ctx.ebx + 0x20); });

			Memory::PatchBytes(CameraVFOVInstructionsScansResult[CameraVFOV2Scan] + 6, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraVFOVInstructionsScansResult[CameraVFOV2Scan] + 6, [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(ctx.esi + 0x20); });

			Memory::PatchBytes(CameraVFOVInstructionsScansResult[CameraVFOV3Scan] + 6, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstruction3Hook = safetyhook::create_mid(CameraVFOVInstructionsScansResult[CameraVFOV3Scan] + 6, [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(ctx.esi + 0x20); });

			Memory::PatchBytes(CameraVFOVInstructionsScansResult[CameraVFOV4Scan] + 6, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstruction4Hook = safetyhook::create_mid(CameraVFOVInstructionsScansResult[CameraVFOV4Scan] + 6, [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(ctx.esi + 0x20); });

			Memory::PatchBytes(CameraVFOVInstructionsScansResult[CameraVFOV5Scan] + 6, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstruction5Hook = safetyhook::create_mid(CameraVFOVInstructionsScansResult[CameraVFOV5Scan] + 6, [](SafetyHookContext& ctx) { CameraVFOVInstructionMidHook(ctx.esi + 0x20); });
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