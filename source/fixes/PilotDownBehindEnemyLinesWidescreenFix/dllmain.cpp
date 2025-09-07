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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "PilotDownBehindEnemyLinesWidescreenFix";
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
constexpr float fNewAspectRatio2 = 0.75f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewCameraFOV2;
float fNewAspectRatio;
float fFOVFactor;
float fAspectRatioScale;
float fNewCameraHFOV;

// Game detection
enum class Game
{
	PDBELGAME,
	PDBELCONFIG,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PDBELGAME, {"Pilot Down: Behind Enemy Lines", "BEL.exe"}},
	{Game::PDBELCONFIG, {"Pilot Down: Behind Enemy Lines - Launcher", "PDLauncher_e.exe"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};

void CameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	// float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ecx + 0xE0);

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esp + 0x4);

	fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid AspectRatioInstruction1Hook{};

void AspectRatioInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewAspectRatio2]
	}
}

static SafetyHookMid CameraHFOVInstructionHook{};

void CameraHFOVInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV]
	}
}

void FOVFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::PDBELCONFIG)
		{
			std::uint8_t* Resolution1024x768ScanResult = Memory::PatternScan(exeModule, "31 00 30 00 32 00 34 00 20 00 78 00 20 00 37 00 36 00 38");
			if (Resolution1024x768ScanResult)
			{
				spdlog::info("Resolution 1024x768 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution1024x768ScanResult - (std::uint8_t*)exeModule);

				Memory::WriteNumberAsChar16Digits(Resolution1024x768ScanResult, iCurrentResX);

				Memory::WriteNumberAsChar16Digits(Resolution1024x768ScanResult + 14, iCurrentResY);
			}
			else
			{
				spdlog::error("Failed to locate resolution 1024x768 scan memory address.");
				return;
			}

			std::uint8_t* Resolution1024x768Scan2Result = Memory::PatternScan(exeModule, "2D 72 65 73 3A 31 30 32 34 2C 37 36 38");
			if (Resolution1024x768Scan2Result)
			{
				spdlog::info("Resolution 1024x768 Scan 2: Address is {:s}+{:x}", sExeName.c_str(), Resolution1024x768Scan2Result - (std::uint8_t*)exeModule);

				if (Maths::digitCount(iCurrentResX) == 4 && (Maths::digitCount(iCurrentResY) == 4 || Maths::digitCount(iCurrentResY) == 3))
				{
					Memory::WriteNumberAsChar8Digits(Resolution1024x768Scan2Result + 5, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(Resolution1024x768Scan2Result + 10, iCurrentResY);
				}

				if (Maths::digitCount(iCurrentResX) == 3 && Maths::digitCount(iCurrentResY) == 3)
				{
					Memory::WriteNumberAsChar8Digits(Resolution1024x768Scan2Result + 5, iCurrentResX);

					Memory::PatchBytes(Resolution1024x768Scan2Result + 8, "\x2C", 1);

					Memory::WriteNumberAsChar8Digits(Resolution1024x768Scan2Result + 9, iCurrentResY);

					Memory::PatchBytes(Resolution1024x768Scan2Result + 12, "\x00", 1);
				}
				
			}
			else
			{
				spdlog::error("Failed to locate resolution 1024x768 scan 2 memory address.");
				return;
			}
		}

		if (eGameType == Game::PDBELGAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::uint8_t* AspectRatioInstruction1ScanResult = Memory::PatternScan(exeModule, "DB 05 ?? ?? ?? ?? 51 8B C1 DA 35 ?? ?? ?? ?? D9 5C 24 04 D9 44 24 18");
			if (AspectRatioInstruction1ScanResult)
			{
			    spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction1ScanResult - (std::uint8_t*)exeModule);;

				Memory::PatchBytes(AspectRatioInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstruction1ScanResult, AspectRatioInstruction1MidHook);

				Memory::PatchBytes(AspectRatioInstruction1ScanResult + 9, "\x90\x90\x90\x90\x90\x90", 6);
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction 1 memory address.");
				return;
			}

			std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 81 E0 00 00 00 C3 CC CC CC CC CC CC CC CC CC");
			if (CameraFOVInstruction1ScanResult)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);;

				fNewCameraFOV = 3.1f;

				Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, CameraFOVInstruction1MidHook);
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
				return;
			}

			std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 44 24 04 D9 F2 DD D8 D9 5C 24 04");
			if (CameraFOVInstruction2ScanResult)
			{
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);;

				Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90", 4);

				CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, CameraFOVInstruction2MidHook);
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
				return;
			}

			std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 90 B3 5D 00 D8 74 24 04 D9 18 D9 05 90 B3 5D 00 D8 F2 D9 58 14 D9 50 28");
			if (CameraHFOVInstructionScanResult)
			{
				spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

				fNewCameraHFOV = 1.0f / fAspectRatioScale;

				Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				CameraHFOVInstructionHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, CameraHFOVInstructionMidHook);
			}
			else
			{
				spdlog::error("Failed to locate camera HFOV instruction memory address.");
				return;
			}

			/*
			std::uint8_t* CodecaveScanResult = Memory::PatternScan(exeModule, "00 30 01 00 00 E0 23 00 00 00 00 00 00 00 00 00");
			if (CodecaveScanResult)
			{
				spdlog::info("Codecave Scan: Address is {:s}+{:x}", sExeName.c_str(), CodecaveScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CodecaveScanResult + 224, "\xDB\x05\xC8\x70\x66\x00\x8B\xC1\x33\xC9\xDA\x35\xC4\x70\x66\x00\xC7\x40\x2C\x00\x00\x80\x3F\x89\x48\x3C\x89\x48\x34\x89\x48\x30\x89\x48\x24\x89\x48\x20\x89\x48\x1C\x89\x48\x18\x89\x48\x10\x89\x48\x0C\x89\x48\x08\x89\x48\x04\xD8\x4C\x24\x0C\xD8\x0D\xCC\xC2\x60\x00\xD9\xF2\xDD\xD8\xD8\x3D\x44\xC4\x60\x00\xD9\x44\x24\x08\xD8\x64\x24\x04\xD8\x7C\x24\x08\xD9\x44\x24\x0C\xD8\x0D\xCC\xC2\x60\x00\xD9\xF2\xDD\xD8\xD8\x3D\x44\xC4\x60\x00\xD9\x18\xD9\xC9\xD9\x58\x14\xD9\x50\x28\xD8\x4C\x24\x04\xD9\xE0\xD9\x58\x38\xC2\x0C\x00", 130);
			}
			else
			{
				spdlog::error("Failed to locate codecave scan memory address.");
				return;
			}

			std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(exeModule, "E8 59 7B FC FF D9 05 94 B1 66 00 D8 4E 08 D9 05 98 B1 66 00 D8 4E 04 DE E9");
			if (AspectRatioInstruction2ScanResult)
			{
				spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction2ScanResult - (std::uint8_t*)exeModule);

				uint8_t* callSite = AspectRatioInstruction2ScanResult;

				uint8_t* codecaveTarget = CodecaveScanResult + 224;

				std::array<uint8_t, 5> saved{};

				if (Memory::PatchCallRel32(callSite, codecaveTarget, &saved) == false)
				{
					spdlog::error("PatchCallRel32 failed (rel32 overflow or not a call).");
				}
				else
				{
					spdlog::info("Successfully patched the CALL instruction located at {:s}+{:x} to the patched codecave located at {:s}+{:x}", sExeName.c_str(), callSite - (uint8_t*)exeModule, sExeName.c_str(), (uintptr_t)codecaveTarget);
				}
			}
			*/
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