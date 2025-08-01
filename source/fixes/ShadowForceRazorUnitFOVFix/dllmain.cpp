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
#include <unordered_set>
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
std::string sFixName = "ShadowForceRazorUnitFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fCameraFOVFactor;
float fWeaponFOVFactor;
float fNewWeaponFOV;
float fNewCameraFOV;
float fNewCameraFOV2;
float fAspectRatioScale;
static uint8_t* WeaponFOVValueAddress;


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
	SFRU,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SFRU, {"Shadow Force: Razor Unit", "Razor.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", fCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", fWeaponFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fCameraFOVFactor);
	spdlog_confparse(fWeaponFOVFactor);

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

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ecx + 0x8);

	if (fCurrentCameraFOV == 90.0f)
	{
		fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV) * fCameraFOVFactor;
	}
	else
	{
		fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::SFRU && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 41 10 52 8B 51 0C 50 8B 45 08 52 50 E8 51 FD FF FF");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid AspectRatioInstructionMidHook{};
			
			AspectRatioInstructionMidHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.ecx + 0x10);

				if (fCurrentAspectRatio = 1.0f / fAspectRatioScale)
				{
					fCurrentAspectRatio *= fAspectRatioScale;
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 41 08 D8 0D ?? ?? ?? ?? 8B 41 14 8B 51 18 89 44 24 00 52 D9 C0 D9 FE");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate gameplay camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 42 08 89 83 20 01 00 00 8B 4A 0C 89 8B 24 01 00 00 8B 42 10 89 83 28 01 00 00 8B 4A 14 89 8B 2C 01 00 00 8B 42 18 89 83 30 01 00 00");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid CameraFOVInstruction2MidHook{};

			CameraFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.edx + 0x8);

				if (fCurrentCameraFOV2 == 90.0f)
				{
					fNewCameraFOV2 = CalculateNewFOV(fCurrentCameraFOV2) * fCameraFOVFactor;
				}
				else
				{
					fNewCameraFOV2 = CalculateNewFOV(fCurrentCameraFOV2);
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV2);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* WeaponFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 83 C6 38 51 8B CE 89 45 EC E8 ?? ?? ?? ??");
		if (WeaponFOVInstructionScanResult)
		{
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), WeaponFOVInstructionScanResult - (std::uint8_t*)exeModule);

			uint32_t imm = *reinterpret_cast<uint32_t*>(WeaponFOVInstructionScanResult + 2);

			WeaponFOVValueAddress = reinterpret_cast<uint8_t*>(imm);

			Memory::PatchBytes(WeaponFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid WeaponFOVInstructionMidHook{};

			WeaponFOVInstructionMidHook = safetyhook::create_mid(WeaponFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentWeaponFOV = *reinterpret_cast<float*>(WeaponFOVValueAddress);

				fNewWeaponFOV = fCurrentWeaponFOV * fWeaponFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewWeaponFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate weapon FOV instruction memory address.");
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