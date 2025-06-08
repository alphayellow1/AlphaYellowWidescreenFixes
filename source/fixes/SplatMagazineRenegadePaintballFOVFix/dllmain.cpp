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
std::string sFixName = "SplatMagazineRenegadePaintballFOVFix";
std::string sFixVersion = "1.1";
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

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fCameraFOVFactor;
float fWeaponFOVFactor;
float fNewCameraZoomFOV;
float fAspectRatioScale;

// Game detection
enum class Game
{
	SMRP,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SMRP, {"SPLAT Magazine: Renegade Paintball", "PaintballGame.exe"}},
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
	return 2.0f * atanf(tanf(fCurrentFOV / 2.0f) * fAspectRatioScale);
}

static SafetyHookMid CameraZoomFOVInstructionHook{};

void CameraZoomFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fNewCameraZoomFOV = CalculateNewFOV(0.7070000172f); // 40.5 degrees in radians

	_asm
	{
		fld dword ptr ds : [fNewCameraZoomFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::SMRP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fNewAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* BriefingCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 48 58 8B 4A 5C 89 48 5C 8B 4A 60 89 48 60 8D 72 64");
		if (BriefingCameraFOVInstructionScanResult)
		{
			spdlog::info("Briefing Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), BriefingCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid BriefingCameraFOVInstructionMidHook{};

			BriefingCameraFOVInstructionMidHook = safetyhook::create_mid(BriefingCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentBriefingCameraFOV = std::bit_cast<float>(ctx.ecx);

				if (fCurrentBriefingCameraFOV == 0.5142856836f) // 29.5 degrees in radians
				{
					fCurrentBriefingCameraFOV = CalculateNewFOV(fCurrentBriefingCameraFOV);
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fCurrentBriefingCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate briefing camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVHipfireInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 83 9C 01 00 00 8D 44 24 14");
		if (CameraFOVHipfireInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Hipfire Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVHipfireInstruction1ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraFOVHipfireInstruction1MidHook{};

			CameraFOVHipfireInstruction1MidHook = safetyhook::create_mid(CameraFOVHipfireInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOVHipfireValue1 = *reinterpret_cast<float*>(ctx.ebx + 0x19C);

				fCurrentCameraFOVHipfireValue1 = fCameraFOVFactor * CalculateNewFOV(1.5707963268f); // 90 degrees in radians
			});
		}
		else
		{
			spdlog::error("Failed to locate first camera FOV hipfire instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVHipfireInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 83 9C 01 00 00 50 D8 E1 51");
		if (CameraFOVHipfireInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Hipfire Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVHipfireInstruction2ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraFOVHipfireInstruction2MidHook{};

			CameraFOVHipfireInstruction2MidHook = safetyhook::create_mid(CameraFOVHipfireInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHipfireFOVValue2 = *reinterpret_cast<float*>(ctx.ebx + 0x19C);

				fCurrentCameraHipfireFOVValue2 = fCameraFOVFactor * CalculateNewFOV(1.5707963268f); // 90 degrees in radians
			});
		}
		else
		{
			spdlog::error("Failed to locate second camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraZoomFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 64 24 5C 5F D8 4C 24 68");
		if (CameraZoomFOVInstructionScanResult)
		{
			spdlog::info("Camera Zoom FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraZoomFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraZoomFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraZoomFOVInstructionHook = safetyhook::create_mid(CameraZoomFOVInstructionScanResult + 6, CameraZoomFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera zoom FOV instruction memory address.");
			return;
		}

		std::uint8_t* WeaponFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 48 04 33 C0 89 44 24 10 89 44 24 14 89 44 24 18 89 44 24 1C 89 44 24 20 89 44 24 24 89 44 24 28 89 44 24 2C 89 44 24 30 89 44 24 34 89 44 24 38 89 44 24 3C");
		if (WeaponFOVInstructionScanResult)
		{
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), WeaponFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid WeaponFOVInstruction2MidHook{};

			WeaponFOVInstruction2MidHook = safetyhook::create_mid(WeaponFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentWeaponFOVValue = *reinterpret_cast<float*>(ctx.eax + 0x4);

				fCurrentWeaponFOVValue = fWeaponFOVFactor * CalculateNewFOV(1.221730471f); // 70 degrees in radians
			});
		}
		else
		{
			spdlog::error("Failed to locate weapon FOV instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatio1ScanResult = Memory::PatternScan(exeModule, "C7 40 64 AB AA AA 3F DD 05 B0 B7 8A");
		if (AspectRatio1ScanResult)
		{
			spdlog::info("Aspect Ratio 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio1ScanResult + 3 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio1ScanResult + 3, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatio2ScanResult = Memory::PatternScan(exeModule, "00 C7 43 64 AB AA AA 3F 8B 80 00 01 00");
		if (AspectRatio2ScanResult)
		{
			spdlog::info("Aspect Ratio 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio2ScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio2ScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatio3ScanResult = Memory::PatternScan(exeModule, "14 C7 42 64 AB AA AA 3F 8B 06 8B 54");
		if (AspectRatio3ScanResult)
		{
			spdlog::info("Aspect Ratio 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio3ScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio3ScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 3 memory address.");
			return;
		}

		std::uint8_t* AspectRatio4ScanResult = Memory::PatternScan(exeModule, "00 C7 41 64 AB AA AA 3F EB 16 A1 64 6C A0");
		if (AspectRatio4ScanResult)
		{
			spdlog::info("Aspect Ratio 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio4ScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio4ScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 4 memory address.");
			return;
		}

		std::uint8_t* AspectRatio5ScanResult = Memory::PatternScan(exeModule, "24 8C 00 00 00 AB AA AA 3F C6 84 24");
		if (AspectRatio5ScanResult)
		{
			spdlog::info("Aspect Ratio 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio5ScanResult + 5 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio5ScanResult + 5, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 5 memory address.");
			return;
		}

		std::uint8_t* AspectRatio6ScanResult = Memory::PatternScan(exeModule, "64 FC FF C7 40 64 AB AA AA 3F DD 05 F0 B4 8A");
		if (AspectRatio6ScanResult)
		{
			spdlog::info("Aspect Ratio 6: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio6ScanResult + 6 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio6ScanResult + 6, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 6 memory address.");
			return;
		}

		std::uint8_t* AspectRatio7ScanResult = Memory::PatternScan(exeModule, "4C C7 41 3C AB AA AA 3F 8B 55 4C 8D 86 EC");
		if (AspectRatio7ScanResult)
		{
			spdlog::info("Aspect Ratio 7: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio7ScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio7ScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 7 memory address.");
			return;
		}

		std::uint8_t* AspectRatio8ScanResult = Memory::PatternScan(exeModule, "00 C7 40 3C AB AA AA 3F 8B 45 50 8B 54 24");
		if (AspectRatio8ScanResult)
		{
			spdlog::info("Aspect Ratio 8: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio8ScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio8ScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 8 memory address.");
			return;
		}

		std::uint8_t* AspectRatio9ScanResult = Memory::PatternScan(exeModule, "74 C7 43 3C AB AA AA 3F D8 0D 38 B0 8A");
		if (AspectRatio9ScanResult)
		{
			spdlog::info("Aspect Ratio 9: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio9ScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio9ScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 9 memory address.");
			return;
		}

		std::uint8_t* AspectRatio10ScanResult = Memory::PatternScan(exeModule, "C7 42 3C AB AA AA 3F 8B C8 89 4A 48 89 4A");
		if (AspectRatio10ScanResult)
		{
			spdlog::info("Aspect Ratio 10: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio10ScanResult + 3 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio10ScanResult + 3, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 10 memory address.");
			return;
		}

		std::uint8_t* AspectRatio11ScanResult = Memory::PatternScan(exeModule, "6C 16 AD 3F AB AA AA 3F 35 58 A8 3F");
		if (AspectRatio11ScanResult)
		{
			spdlog::info("Aspect Ratio 11: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio11ScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio11ScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 11 memory address.");
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