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
std::string sFixName = "FootballGenerationWidescreenFix";
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

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewAspectRatio2;
float fFOVFactor;
float fNewCameraHFOV;
float fNewGameplayCameraFOV;
float fAspectRatioScale;

// Game detection
enum class Game
{
	FGSETUP,
	FGGAME,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FGSETUP, {"Football Generation - Setup", "SGame.exe"}},
	{Game::FGGAME, {"Football Generation", "Game.exe"}},
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

static SafetyHookMid GameplayAspectRatioInstructionHook{};

void GameplayAspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio2]
	}
}

static SafetyHookMid GameplayCameraFOVInstructionHook{};

void GameplayCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	// Reference the dereferenced [ESP+18] address that holds the current gameplay camera FOV
	float& fCurrentGameplayCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x18);

	// Compute the new FOV value
	fNewGameplayCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentGameplayCameraFOV, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewGameplayCameraFOV]
	}
}

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::FGSETUP)
		{
			// If both width and height values have 4 digits, then write the resolution values in place of the 1280x1024 one
			if (Maths::digitCount(iCurrentResX) == 4 && Maths::digitCount(iCurrentResY) == 4)
			{
				std::uint8_t* Resolution1280x1024ScanResult = Memory::PatternScan(exeModule, "31 32 38 30 78 31 30 32 34 20 33 32 20 62 69 74 73 00 00 00 31 32 38 30 78 31 30 32 34");
				if (Resolution1280x1024ScanResult)
				{
					spdlog::info("Resolution 1280x1024 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution1280x1024ScanResult - (std::uint8_t*)exeModule);

					Memory::WriteNumberAsChar8Digits(Resolution1280x1024ScanResult, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(Resolution1280x1024ScanResult + 5, iCurrentResY);

					Memory::WriteNumberAsChar8Digits(Resolution1280x1024ScanResult + 20, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(Resolution1280x1024ScanResult + 25, iCurrentResY);
				}
				else
				{
					spdlog::error("Failed to locate resolution 1280x1024 scan memory address.");
					return;
				}
			}

			// If width has 4 digits and height has 3 digits, then write the resolution values in place of the 1024x768 one
			if (Maths::digitCount(iCurrentResX) == 4 && Maths::digitCount(iCurrentResY) == 3)
			{
				std::uint8_t* Resolution1024x768ScanResult = Memory::PatternScan(exeModule, "31 30 32 34 78 37 36 38 20 33 32 20 62 69 74 73 00 00 00 00 31 30 32 34 78 37 36 38");
				if (Resolution1024x768ScanResult)
				{
					spdlog::info("Resolution 1024x768 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution1024x768ScanResult - (std::uint8_t*)exeModule);

					Memory::WriteNumberAsChar8Digits(Resolution1024x768ScanResult, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(Resolution1024x768ScanResult + 5, iCurrentResY);

					Memory::WriteNumberAsChar8Digits(Resolution1024x768ScanResult + 20, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(Resolution1024x768ScanResult + 25, iCurrentResY);
				}
				else
				{
					spdlog::error("Failed to locate resolution 1024x768 scan memory address.");
					return;
				}
			}

			// If both width and height values have 3 digits, then write the resolution values in place of the 800x600 one
			if (Maths::digitCount(iCurrentResX) == 3 && Maths::digitCount(iCurrentResY) == 3)
			{
				std::uint8_t* Resolution800x600ScanResult = Memory::PatternScan(exeModule, "38 30 30 78 36 30 30 20 33 32 20 62 69 74 73 00 38 30 30 78 36 30 30");
				if (Resolution800x600ScanResult)
				{
					spdlog::info("Resolution 800x600 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution800x600ScanResult - (std::uint8_t*)exeModule);

					Memory::WriteNumberAsChar8Digits(Resolution800x600ScanResult, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(Resolution800x600ScanResult + 4, iCurrentResY);

					Memory::WriteNumberAsChar8Digits(Resolution800x600ScanResult + 16, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(Resolution800x600ScanResult + 20, iCurrentResY);
				}
				else
				{
					spdlog::error("Failed to locate resolution 800x600 scan memory address.");
					return;
				}
			}
		}

		if (eGameType == Game::FGGAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 4E 68 8B 50 04 D8 76 68 89 56 6C");
			if (CameraHFOVInstructionScanResult)
			{
				spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90", 3);

				static SafetyHookMid CameraHFOVInstructionMidHook{};

				CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					// Convert the ECX register value to float
					float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

					if (fCurrentCameraHFOV == 0.1763269752f) // Menu HFOV
					{
						fCurrentCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale);
					}
					else
					{
						fNewCameraHFOV = fCurrentCameraHFOV;
					}

					*reinterpret_cast<float*>(ctx.esi + 0x68) = fNewCameraHFOV;
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera HFOV instruction memory address.");
				return;
			}

			std::uint8_t* GameplayAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D9 5C 24 10 EB 58 8B 46 24");
			if (GameplayAspectRatioInstructionScanResult)
			{
				spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

				fNewAspectRatio2 = 0.75f * (1.0f / fAspectRatioScale);

				Memory::PatchBytes(GameplayAspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				GameplayAspectRatioInstructionHook = safetyhook::create_mid(GameplayAspectRatioInstructionScanResult, GameplayAspectRatioInstructionMidHook);
			}
			else
			{
				spdlog::info("Cannot locate the gameplay aspect ratio instruction memory address.");
				return;
			}

			std::uint8_t* GameplayCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 18 D8 0D ?? ?? ?? ?? D9 F2 DD D8 D9 54 24 0C");
			if (GameplayCameraFOVInstructionScanResult)
			{
				spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GameplayCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(GameplayCameraFOVInstructionScanResult, "\x90\x90\x90\x90", 4);

				GameplayCameraFOVInstructionHook = safetyhook::create_mid(GameplayCameraFOVInstructionScanResult, GameplayCameraFOVInstructionMidHook);
			}
			else
			{
				spdlog::info("Cannot locate the gameplay camera FOV instruction memory address.");
				return;
			}
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