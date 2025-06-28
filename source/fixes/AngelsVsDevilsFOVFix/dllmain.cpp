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
constexpr float fTolerance = 0.000001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraHFOV2;
float fNewCameraHFOV3;
float fNewCameraHFOV4;
float fNewCameraHFOV5;
float fNewCameraVFOV;
float fNewCameraVFOV2;
float fNewCameraVFOV3;
float fNewCameraVFOV4;
float fNewCameraVFOV5;

// Game detection
enum class Game
{
	AVD,
	Unknown
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

float CalculateNewHFOVWithoutFOVFactor(float fCurrentHFOV)
{
	return 2.0f * atanf((tanf(fCurrentHFOV / 2.0f)) * fAspectRatioScale);
}

float CalculateNewHFOVWithFOVFactor(float fCurrentHFOV)
{
	return 2.0f * atanf((fFOVFactor * tanf(fCurrentHFOV / 2.0f)) * fAspectRatioScale);
}

float CalculateNewVFOVWithoutFOVFactor(float fCurrentVFOV)
{
	return 2.0f * atanf(tanf(fCurrentVFOV / 2.0f));
}

float CalculateNewVFOVWithFOVFactor(float fCurrentVFOV)
{
	return 2.0f * atanf(fFOVFactor * tanf(fCurrentVFOV / 2.0f));
}

static SafetyHookMid CameraHFOVInstructionHook{};	

void CameraHFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.ebx + 0x1C);

	if (fCurrentCameraHFOV == 0.7853981853f)
	{
		fNewCameraHFOV = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV);
	}
	else
	{
		fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV]
	}
}

static SafetyHookMid CameraHFOVInstruction2Hook{};

void CameraHFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x1C);

	if (fCurrentCameraHFOV2 == 0.7853981853f)
	{
		fNewCameraHFOV2 = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV2);
	}
	else
	{
		fNewCameraHFOV2 = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV2);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV2]
	}
}

static SafetyHookMid CameraHFOVInstruction3Hook{};

void CameraHFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV3 = *reinterpret_cast<float*>(ctx.esi + 0x1C);

	if (fCurrentCameraHFOV3 == 0.7853981853f)
	{
		fNewCameraHFOV3 = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV3);
	}
	else
	{
		fNewCameraHFOV3 = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV3);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV3]
	}
}

static SafetyHookMid CameraHFOVInstruction4Hook{};

void CameraHFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV4 = *reinterpret_cast<float*>(ctx.esi + 0x1C);

	if (fCurrentCameraHFOV4 == 0.7853981853f)
	{
		fNewCameraHFOV4 = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV4);
	}
	else
	{
		fNewCameraHFOV4 = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV4);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV4]
	}
}

static SafetyHookMid CameraHFOVInstruction5Hook{};

void CameraHFOVInstruction5MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV5 = *reinterpret_cast<float*>(ctx.esi + 0x1C);

	if (fCurrentCameraHFOV5 == 0.7853981853f)
	{
		fNewCameraHFOV5 = CalculateNewHFOVWithFOVFactor(fCurrentCameraHFOV5);
	}
	else
	{
		fNewCameraHFOV5 = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV5);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV5]
	}
}

static SafetyHookMid CameraVFOVInstructionHook{};

void CameraVFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.ebx + 0x20);

	if (fabsf(fCurrentCameraVFOV - (0.589048624f / fAspectRatioScale)) < fTolerance)
	{
		fNewCameraVFOV = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);
	}
	else
	{
		fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV * fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV]
	}
}

static SafetyHookMid CameraVFOVInstruction2Hook{};

void CameraVFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraVFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x20);

	if (fabsf(fCurrentCameraVFOV2 - (0.589048624f / fAspectRatioScale)) < fTolerance)
	{
		fNewCameraVFOV2 = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV2 * fAspectRatioScale);
	}
	else
	{
		fNewCameraVFOV2 = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV2 * fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV2]
	}
}

static SafetyHookMid CameraVFOVInstruction3Hook{};

void CameraVFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraVFOV3 = *reinterpret_cast<float*>(ctx.esi + 0x20);

	if (fabsf(fCurrentCameraVFOV3 - (0.589048624f / fAspectRatioScale)) < fTolerance)
	{
		fNewCameraVFOV3 = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV3 * fAspectRatioScale);
	}
	else
	{
		fNewCameraVFOV3 = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV3 * fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV3]
	}
}

static SafetyHookMid CameraVFOVInstruction4Hook{};

void CameraVFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraVFOV4 = *reinterpret_cast<float*>(ctx.esi + 0x20);

	if (fabsf(fCurrentCameraVFOV4 - (0.589048624f / fAspectRatioScale)) < fTolerance)
	{
		fNewCameraVFOV4 = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV4 * fAspectRatioScale);
	}
	else
	{
		fNewCameraVFOV4 = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV4 * fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV4]
	}
}

static SafetyHookMid CameraVFOVInstruction5Hook{};

void CameraVFOVInstruction5MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraVFOV5 = *reinterpret_cast<float*>(ctx.esi + 0x20);

	if (fabsf(fCurrentCameraVFOV5 - (0.589048624f / fAspectRatioScale)) < fTolerance)
	{
		fNewCameraVFOV5 = CalculateNewVFOVWithFOVFactor(fCurrentCameraVFOV5 * fAspectRatioScale);
	}
	else
	{
		fNewCameraVFOV5 = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV5 * fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV5]
	}
}

void FOVFix()
{
	if (eGameType == Game::AVD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraHFOVInstructionResult = Memory::PatternScan(exeModule, "D9 43 1C D8 0D ?? ?? ?? ?? D9 F2 DD D8");
		if (CameraHFOVInstructionResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionResult, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraHFOVInstructionResult, CameraHFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction2Result = Memory::PatternScan(exeModule, "D9 46 1C 8B 18 D8 0D ?? ?? ?? ?? 8B 68 04 89 5C 24 14 89 6C 24 18 8B 48 08");
		if (CameraHFOVInstruction2Result)
		{
			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction2Result - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraHFOVInstruction2Result, "\x90\x90\x90", 3); // NOP out the original instruction
			
			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstruction2Result, CameraHFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction3Result = Memory::PatternScan(exeModule, "D9 46 1C D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51 C7 44 24 54 00 00 00 00 8B 44 24 54 C7 44 24 5C 00 00 00 00 8B 54 24 5C 89 44 24 18 D9 1C 24 89 4C 24 1C 89 54 24 20 E8 ?? ?? ?? ?? 83 EC 08 8D 4C 24 68 8B C4");
		if (CameraHFOVInstruction3Result)
		{
			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction3Result - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraHFOVInstruction3Result, "\x90\x90\x90", 3); // NOP out the original instruction
			
			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstruction3Result, CameraHFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction4Result = Memory::PatternScan(exeModule, "D9 46 1C D8 0D ?? ?? ?? ?? 51 D9 1C 24 E8 ?? ?? ?? ?? 83 EC 08 8B C4 83 EC 0C 8B CC D9 10 D9 50 04 D9 58 08");
		if (CameraHFOVInstruction4Result)
		{
			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction4Result - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction4Result, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraHFOVInstruction4Result, CameraHFOVInstruction4MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction5Result = Memory::PatternScan(exeModule, "D9 46 1C D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51 C7 44 24 54 00 00 00 00 8B 44 24 54 C7 44 24 5C 00 00 00 00 8B 54 24 5C 89 44 24 18 D9 1C 24 89 4C 24 1C 89 54 24 20 E8 ?? ?? ?? ?? 83 EC 08 8D 4C 24 74 8B C4 6A 00 6A 00 D9 10 D9 50 04 D9 58 08");
		if (CameraHFOVInstruction5Result)
		{
			spdlog::info("Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction5Result - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraHFOVInstruction5Result, "\x90\x90\x90", 3); // NOP out the original instruction
			
			CameraHFOVInstruction5Hook = safetyhook::create_mid(CameraHFOVInstruction5Result, CameraHFOVInstruction5MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 5 memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 5D 00 D9 43 20 D8 0D ?? ?? ?? ?? D9 F2 DD D8");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult + 3 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionScanResult + 3, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraVFOVInstructionScanResult + 3, CameraVFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 9E C8 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? 51 D9 1C 24 E8 ?? ?? ?? ?? 83 EC 08");
		if (CameraVFOVInstruction2ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction2ScanResult + 3 - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraVFOVInstruction2ScanResult + 6, "\x90\x90\x90", 3); // NOP out the original instruction
			
			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraVFOVInstruction2ScanResult + 6, CameraVFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D9 9E 98 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51");
		if (CameraVFOVInstruction3ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction3ScanResult + 6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstruction3ScanResult + 6, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstruction3Hook = safetyhook::create_mid(CameraVFOVInstruction3ScanResult + 6, CameraVFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "D9 9E 98 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? 51 D9 1C 24 E8 ?? ?? ?? ?? 83 EC 08 8B C4");
		if (CameraVFOVInstruction4ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction4ScanResult + 6 - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraVFOVInstruction4ScanResult + 6, "\x90\x90\x90", 3); // NOP out the original instruction
			
			CameraVFOVInstruction4Hook = safetyhook::create_mid(CameraVFOVInstruction4ScanResult + 6, CameraVFOVInstruction4MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstruction5ScanResult = Memory::PatternScan(exeModule, "D9 9E A8 00 00 00 D9 46 20 D8 0D ?? ?? ?? ?? C7 44 24 54 00 00 00 00 8B 4C 24 54 51 C7 44 24 54 00 00 00 00 8B 44 24 54");
		if (CameraVFOVInstruction5ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction5ScanResult + 6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstruction5ScanResult + 6, "\x90\x90\x90", 3); // NOP out the original instruction

			CameraVFOVInstruction5Hook = safetyhook::create_mid(CameraVFOVInstruction5ScanResult + 6, CameraVFOVInstruction5MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction 5 memory address.");
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