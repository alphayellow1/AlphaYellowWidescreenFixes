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
std::string sFixName = "WhiteoutWidescreenFix";
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

// Ini variables
bool bFixActive;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewAspectRatio;
float fAspectRatioScale;
float fFOVFactor;
float fNewCameraHFOV;
float fNewCameraHFOV2;
float fNewCameraHFOV3;
float fNewCameraHFOV4;
float fNewCameraVFOV;
float fNewCameraVFOV2;
float fNewCameraVFOV3;
float fNewCameraVFOV4;

// Game detection
enum class Game
{
	WHITEOUT,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WHITEOUT, {"Whiteout", "Whiteout.exe"}},
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

static SafetyHookMid CameraHFOVInstruction3Hook{};

void CameraHFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	// Store the current HFOV value from the memory address [ESI + 0x1E8]
	float fCurrentCameraHFOV3 = *reinterpret_cast<float*>(ctx.esi + 0x1E8);

	// Race gameplay and pause camera HFOVs
	if (Maths::isClose(fCurrentCameraHFOV3, 0.9336100817f) || Maths::isClose(fCurrentCameraHFOV3, 0.9336093068f))
	{
		fNewCameraHFOV3 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV3, fAspectRatioScale) * fFOVFactor;
	}
	// Rest of HUD elements and cameras (main menu mainly and some HUD elements)
	else
	{
		fNewCameraHFOV3 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV3, fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV3] // Load the new HFOV value 3 into the FPU stack
	}
}

static SafetyHookMid CameraHFOVInstruction4Hook{};

void CameraHFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	// Store the current HFOV value from the memory address [ESI + 0x1E8]
	float fCurrentCameraHFOV4 = *reinterpret_cast<float*>(ctx.esi + 0x1E8);

	// Race gameplay and pause camera HFOVs
	if (Maths::isClose(fCurrentCameraHFOV4, 0.9336100817f) || Maths::isClose(fCurrentCameraHFOV4, 0.9336093068f))
	{
		fNewCameraHFOV4 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV4, fAspectRatioScale) * fFOVFactor;
	}
	// Rest of HUD elements and cameras (main menu mainly and some HUD elements)
	else
	{
		fNewCameraHFOV4 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV4, fAspectRatioScale);
	}

	_asm
	{
		fdiv dword ptr ds:[fNewCameraHFOV4] // Load the new HFOV value 4 into the FPU stack
	}
}

static SafetyHookMid CameraVFOVInstruction3Hook{};

void CameraVFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	fNewCameraVFOV3 = fNewCameraHFOV3 / fNewAspectRatio;

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV3] // Load the new VFOV value into the FPU stack
	}
}

static SafetyHookMid CameraVFOVInstruction4Hook{};

void CameraVFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	fNewCameraVFOV4 = fNewCameraHFOV4 / fNewAspectRatio;

	_asm
	{
		fdiv dword ptr ds:[fNewCameraVFOV4] // Load the new VFOV value 4 into the FPU stack
	}
}

void WidescreenFix()
{
	if (eGameType == Game::WHITEOUT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* MainMenuResolutionScanResult = Memory::PatternScan(exeModule, "BE 80 02 00 00 BF E0 01 00 00 EB 3B A1 A4 35 5E");
		if (MainMenuResolutionScanResult)
		{
			spdlog::info("Main Menu Resolution Scan: Address is {:s}+{:x}", sExeName.c_str(), MainMenuResolutionScanResult - (std::uint8_t*)exeModule);

			// Default menu resolution is 640x480
			Memory::Write(MainMenuResolutionScanResult + 1, iCurrentResX);

			Memory::Write(MainMenuResolutionScanResult + 6, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the main menu resolution scan memory address.");
			return;
		}

		std::uint8_t* GameplayResolutionScanResult = Memory::PatternScan(exeModule, "80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00");
		if (GameplayResolutionScanResult)
		{
			spdlog::info("Gameplay Resolution Scan: Address is {:s}+{:x}", sExeName.c_str(), GameplayResolutionScanResult - (std::uint8_t*)exeModule);
			
			// 640x480
			Memory::Write(GameplayResolutionScanResult, iCurrentResX);
			
			Memory::Write(GameplayResolutionScanResult + 4, iCurrentResY);

			// 800x600
			Memory::Write(GameplayResolutionScanResult + 8, iCurrentResX);

			Memory::Write(GameplayResolutionScanResult + 12, iCurrentResY);

			// 1024x768
			Memory::Write(GameplayResolutionScanResult + 16, iCurrentResX);

			Memory::Write(GameplayResolutionScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the gameplay resolution scan memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 91 E8 01 00 00 89 54 24 20");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Store the current HFOV value from the memory address [ECX + 0x1E8]
				float fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.ecx + 0x1E8);

				// Race gameplay and pause camera HFOVs
				if (Maths::isClose(fCurrentCameraHFOV, 0.9336100817f) || Maths::isClose(fCurrentCameraHFOV, 0.9336093068f))
				{
				    fNewCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale) * fFOVFactor;
				}
				// Rest of HUD elements and cameras (main menu mainly and some HUD elements)
				else
				{
					fNewCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale);
				}

				ctx.edx = std::bit_cast<std::uintptr_t>(fNewCameraHFOV); // Update EDX with the new HFOV value
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 8E E8 01 00 00 89 4C 24 14");
		if (CameraHFOVInstruction2ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraHFOVInstruction2MidHook{};

			CameraHFOVInstruction2MidHook = safetyhook::create_mid(CameraHFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				// Store the current HFOV value from the memory address [ESI + 0x1E8]
				float fCurrentCameraHFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x1E8);

				// Race gameplay and pause camera HFOVs
				if (Maths::isClose(fCurrentCameraHFOV2, 0.9336100817f) || Maths::isClose(fCurrentCameraHFOV2, 0.9336093068f))
				{
					fNewCameraHFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV2, fAspectRatioScale) * fFOVFactor;
				}
				// Rest of HUD elements and cameras (main menu mainly and some HUD elements)
				else
				{
					fNewCameraHFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV2, fAspectRatioScale);
				}

				ctx.ecx = std::bit_cast<std::uintptr_t>(fNewCameraHFOV2); // Update EDX with the new HFOV value 2
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D9 86 E8 01 00 00 D8 B6 08 02 00 00 DE C9");
		if (CameraHFOVInstruction3ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction3ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraHFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstruction3ScanResult, CameraHFOVInstruction3MidHook);				
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "D8 B6 E8 01 00 00 50 D8 8E 08 02 00 00 D9 9E 10 02 00 00");
		if (CameraHFOVInstruction4ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction4ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraHFOVInstruction4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraHFOVInstruction4ScanResult, CameraHFOVInstruction4MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 EC 01 00 00 89 44 24 24");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				fNewCameraVFOV = fNewCameraHFOV / fNewAspectRatio;

				ctx.eax = std::bit_cast<std::uintptr_t>(fNewCameraVFOV); // Update EAX with the new VFOV value
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera VFOV instruction memory address.");
			return;
		}
		
		std::uint8_t* CameraVFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 96 EC 01 00 00 89 54 24 18");
		if (CameraVFOVInstruction2ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraVFOVInstruction2MidHook{};

			CameraVFOVInstruction2MidHook = safetyhook::create_mid(CameraVFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				fNewCameraVFOV2 = fNewCameraHFOV2 / fNewAspectRatio;

				ctx.edx = std::bit_cast<std::uintptr_t>(fNewCameraVFOV2); // Update EDX with the new VFOV value 2
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera VFOV instruction 2 memory address.");
			return;
		}
		
		std::uint8_t* CameraVFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D9 86 EC 01 00 00 D8 B6 0C 02 00 00 52");
		if (CameraVFOVInstruction3ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction3Hook = safetyhook::create_mid(CameraVFOVInstruction3ScanResult, CameraVFOVInstruction3MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera VFOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "D8 B6 EC 01 00 00 D8 8E 0C 02 00 00 D9 9E 14 02 00 00");
		if (CameraVFOVInstruction4ScanResult)
		{
			spdlog::info("Camera VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction4ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraVFOVInstruction4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			CameraVFOVInstruction4Hook = safetyhook::create_mid(CameraVFOVInstruction4ScanResult, CameraVFOVInstruction4MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera VFOV instruction 4 memory address.");
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