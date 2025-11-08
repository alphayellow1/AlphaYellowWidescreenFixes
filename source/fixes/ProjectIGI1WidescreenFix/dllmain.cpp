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
std::string sFixName = "ProjectIGI1WidescreenFix";
std::string sFixVersion = "1.5";
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
constexpr float fOriginalAspectRatio = 4.0f;

// Ini variables
bool bFixActive;

// Variables
int iNewResX;
int iNewResY;
float fCameraFOVFactor;
double dWeaponFOVFactor;
float fNewCameraFOV;
float fNewAspectRatio;
float fNewAspectRatio2;
float fAspectRatioScale;
uint16_t GameVersionCheckValue;
int8_t* iInsideComputer;
uint8_t* WeaponFOVValue1Address;
uint8_t* WeaponFOVValue2Address;
uint8_t* WeaponFOVValue3Address;
bool bInsideComputer;
double dNewWeaponFOV1;
double dNewWeaponFOV2;
double dNewWeaponFOV3;

// Game detection
enum class Game
{
	PI1,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PI1, {"Project IGI 1: I'm Going In", "IGI.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "Width", iNewResX);
	inipp::get_value(ini.sections["Settings"], "Height", iNewResY);
	inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", fCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", dWeaponFOVFactor);
	spdlog_confparse(iNewResX);
	spdlog_confparse(iNewResY);
	spdlog_confparse(fCameraFOVFactor);
	spdlog_confparse(dWeaponFOVFactor);

	// If resolution not specified, use desktop resolution
	if (iNewResX <= 0 || iNewResY <= 0)
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();
		iNewResX = desktopDimensions.first;
		iNewResY = desktopDimensions.second;
		spdlog_confparse(iNewResX);
		spdlog_confparse(iNewResY);
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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	fNewAspectRatio2 = fOriginalAspectRatio * fAspectRatioScale;

	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio2]
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	bInsideComputer = (*iInsideComputer == 1);

	fNewCameraFOV = fAspectRatioScale * fCameraFOVFactor;

	if (bInsideComputer == 0)
	{
		_asm
		{
			fmul dword ptr ds:[fNewCameraFOV]
		}
	}	
}

static SafetyHookMid WeaponFOVInstructionHook{};

void WeaponFOVInstructionMidHook(SafetyHookContext& ctx)
{
	double& dCurrentWeaponFOV1 = *reinterpret_cast<double*>(WeaponFOVValue1Address);

	dNewWeaponFOV1 = Maths::CalculateNewFOV_RadBased(dCurrentWeaponFOV1, fAspectRatioScale, Maths::AngleMode::HalfAngle) * dWeaponFOVFactor;

	_asm
	{
		fld qword ptr ds:[dNewWeaponFOV1]
	}
}

static SafetyHookMid WeaponFOVInstruction2Hook{};

void WeaponFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	double& dCurrentWeaponFOV2 = *reinterpret_cast<double*>(WeaponFOVValue2Address);

	dNewWeaponFOV2 = Maths::CalculateNewFOV_RadBased(dCurrentWeaponFOV2, fAspectRatioScale, Maths::AngleMode::HalfAngle) * dWeaponFOVFactor;

	_asm
	{
		fld qword ptr ds:[dNewWeaponFOV2]
	}
}

static SafetyHookMid WeaponFOVInstruction3Hook{};

void WeaponFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	double& dCurrentWeaponFOV3 = *reinterpret_cast<double*>(WeaponFOVValue3Address);

	dNewWeaponFOV3 = Maths::CalculateNewFOV_RadBased(dCurrentWeaponFOV3, fAspectRatioScale, Maths::AngleMode::HalfAngle) * dWeaponFOVFactor;

	_asm
	{
		fld qword ptr ds:[dNewWeaponFOV3]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::PI1 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iNewResX) / static_cast<float>(iNewResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionScanResult = Memory::PatternScan(exeModule, "8B 45 0C 89 44 24 14 8B 4D 10 89 4C 24 18 88 5C 24 24 8B 55 14 89 5C 24 1C");
		if (ResolutionInstructionScanResult)
		{
			spdlog::info("Resolution Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionInstructionMidHook{};

			ResolutionInstructionMidHook = safetyhook::create_mid(ResolutionInstructionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentResX = *reinterpret_cast<int*>(ctx.ebp + 0xC);

				int& iCurrentResY = *reinterpret_cast<int*>(ctx.ebp + 0x10);

				iCurrentResX = iNewResX;

				iCurrentResY = iNewResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instruction scan memory address.");
			return;
		}

		std::uint8_t* InsideComputerInstructionScanResult = Memory::PatternScan(exeModule, "A2 ?? ?? ?? ?? D9 5C 24 10 F3 A5 D9 E0 D9 44 24 0C D9 E0 D9 44 24 10 D9 E0 D9 44 24 1C D8 C9 D9 44 24 18 D8 CB DE C1 D9 44 24 14 D8 CC DE C1 D9 5C 24 08 D9 44 24 28 D8 C9 D9 44 24 24 D8 CB DE C1 D9 44 24 20 D8 CC DE C1 D9 5C 24 0C D9 44 24 34 D8 C9 D9 44 24 30 D8 CB DE C1 D9 44 24 2C D8 CC DE C1 D9 5C 24 10 8B 44 24 08 B9 0A 00 00 00 8D 74 24 14");
		if (InsideComputerInstructionScanResult)
		{
			spdlog::info("Inside Computer Instruction: Address is{:s} + {:x}", sExeName.c_str(), InsideComputerInstructionScanResult - (std::uint8_t*)exeModule);

			uint8_t* InsideComputerValueAddress = Memory::GetPointerFromAddress<uint32_t>(InsideComputerInstructionScanResult + 1, Memory::PointerMode::Absolute);

			iInsideComputer = reinterpret_cast<int8_t*>(InsideComputerValueAddress);
		}
		else
		{
			spdlog::error("Failed to locate inside computer instruction scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? DB 44 24 00 D8 0D ?? ?? ?? ?? DE F9 59 C3 90");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 1C 24 8D 44 24 48 52 8D 4C 24 74 50 51");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is{:s} + {:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction scan memory address.");
			return;
		}		

		std::uint8_t* WeaponFOVInstructionScanResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 98 E8 01 00 00 C3 90 6A 00");
		if (WeaponFOVInstructionScanResult)
		{
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), WeaponFOVInstructionScanResult - (std::uint8_t*)exeModule);

			WeaponFOVValue1Address = Memory::GetPointerFromAddress<uint32_t>(WeaponFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponFOVInstructionHook = safetyhook::create_mid(WeaponFOVInstructionScanResult, WeaponFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon FOV instruction memory address.");
			return;
		}

		std::uint8_t* WeaponFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 98 E8 01 00 00 89 97 B0 01 00 00 5F 5E 5D 5B");
		if (WeaponFOVInstruction2ScanResult)
		{
			spdlog::info("Weapon FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), WeaponFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			WeaponFOVValue2Address = Memory::GetPointerFromAddress<uint32_t>(WeaponFOVInstruction2ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponFOVInstruction2Hook = safetyhook::create_mid(WeaponFOVInstruction2ScanResult, WeaponFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* WeaponFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 5C 24 18 8B 44 24 18 5F D9 99 E4 01 00 00 5E 5D 89 81 E8 01 00 00");
		if (WeaponFOVInstruction3ScanResult)
		{
			spdlog::info("Weapon FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), WeaponFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			WeaponFOVValue3Address = Memory::GetPointerFromAddress<uint32_t>(WeaponFOVInstruction3ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(WeaponFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponFOVInstruction3Hook = safetyhook::create_mid(WeaponFOVInstruction3ScanResult, WeaponFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon FOV instruction 3 memory address.");
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
