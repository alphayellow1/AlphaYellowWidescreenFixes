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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "BassProShopsTrophyHunter2007WidescreenFix";
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

// Constants
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fAspectRatioScale;
float fFOVFactor;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
float fNewWeaponFOV;
float fNewBulletCameraFOV1;
float fNewBulletCameraFOV2;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
	return degrees * (fPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
	return radians * (180.0f / fPi);
}

// Game detection
enum class Game
{
	BPSTH2007,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BPSTH2007, {"Bass Pro Shops: Trophy Hunter 2007", "BPS Trophy Hunter 2007.exe"}},
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

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * fAspectRatioScale));
}

static SafetyHookMid WeaponFOVInstructionHook{};

void WeaponFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponFOV = *reinterpret_cast<float*>(ctx.ecx + 0x80);

	fNewWeaponFOV = CalculateNewFOV(fCurrentWeaponFOV);

	_asm
	{
		fld dword ptr ds:[fNewWeaponFOV]
	}
}



void WidescreenFix()
{
	if (eGameType == Game::BPSTH2007 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "89 41 1C 8B 44 24 0C 89 51 20 89 41 24 C2 0C 00 CC CC CC CC");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstructionMidHook{};

			ResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x1C) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScanResult + 7, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstructionMidHook{};

			ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 7, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x20) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "68 00 00 8C 42 51 89 85 08 01 00 00 8B 0D ?? ?? ?? ??");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV1 = CalculateNewFOV(70.0f) * fFOVFactor;

			Memory::Write(CameraFOVInstruction1ScanResult + 1, fNewCameraFOV1);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "68 00 00 70 42 8B CD E8 ?? ?? ?? ?? 50 8B CD");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV2 = CalculateNewFOV(70.0f) * fFOVFactor;

			Memory::Write(CameraFOVInstruction2ScanResult + 1, fNewCameraFOV2);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "68 00 00 8C 42 51 8B CC 89 64 24 28 51 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 33 D2 8A 95 C7 00 00 00");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);
			
			fNewCameraFOV3 = CalculateNewFOV(70.0f);
			
			Memory::Write(CameraFOVInstruction3ScanResult + 1, fNewCameraFOV3);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "C7 44 24 34 00 00 82 42 D9 05 ?? ?? ?? ?? D9 44 24 38 DA E9 DF E0 F6 C4 44");
		if (CameraFOVInstruction4ScanResult)
		{
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction4ScanResult - (std::uint8_t*)exeModule);
			
			fNewCameraFOV4 = CalculateNewFOV(65.0f);

			Memory::Write(CameraFOVInstruction4ScanResult + 4, fNewCameraFOV4);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* WeaponFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 81 80 00 00 00 C3 CC CC CC CC CC CC CC CC CC");
		if (WeaponFOVInstructionScanResult)
		{
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), WeaponFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(WeaponFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			WeaponFOVInstructionHook = safetyhook::create_mid(WeaponFOVInstructionScanResult, WeaponFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the weapon FOV instruction memory address.");
			return;
		}

		std::uint8_t* BulletCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "C7 46 38 00 00 B4 42 C7 46 48 9A 99 99 3E 88 86 D4 00 00 00 89 86 D8 00 00 00 5E");
		if (BulletCameraFOVInstructionScanResult)
		{
			spdlog::info("Bullet Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), BulletCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			fNewBulletCameraFOV1 = CalculateNewFOV(90.0f);

			Memory::Write(BulletCameraFOVInstructionScanResult + 3, fNewBulletCameraFOV1);
		}
		else
		{
			spdlog::info("Cannot locate the bullet camera FOV instruction memory address.");
			return;
		}
		
		std::uint8_t* BulletCameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "C7 46 38 00 00 BE 42 89 48 08 8B 15 ?? ?? ?? ?? 89 9E C0 00 00 00");
		if (BulletCameraFOVInstruction2ScanResult)
		{
			spdlog::info("Bullet Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), BulletCameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			fNewBulletCameraFOV2 = CalculateNewFOV(95.0f);

			Memory::Write(BulletCameraFOVInstruction2ScanResult + 3, fNewBulletCameraFOV2);
		}
		else
		{
			spdlog::info("Cannot locate the bullet camera FOV instruction 2 memory address.");
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