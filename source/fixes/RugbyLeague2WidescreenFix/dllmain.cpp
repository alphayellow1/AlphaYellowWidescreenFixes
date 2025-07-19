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
std::string sFixName = "RugbyLeague2WidescreenFix";
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
float fFOVFactor;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fAspectRatioScale;

// Game detection
enum class Game
{
	RL2,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RL2, {"Rugby League 2", "RugbyLeague2.exe"}},
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
	return fCurrentFOV * fAspectRatioScale;
}

void WidescreenFix()
{
	if (eGameType == Game::RL2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionScanResult = Memory::PatternScan(exeModule, "C2 00 C7 02 80 02 00 00 A1 94 10 C2 00 C7 40 04 E0 01 00 00");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScanResult + 4, iCurrentResX);

			Memory::Write(ResolutionScanResult + 16, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan memory address.");
			return;
		}

		std::uint8_t* Resolution2ScanResult = Memory::PatternScan(exeModule, "C6 44 24 1C 05 C7 06 80 02 00 00 C7 46 04 E0 01 00 00");
		if (Resolution2ScanResult)
		{
			spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(Resolution2ScanResult + 7, iCurrentResX);
			
			Memory::Write(Resolution2ScanResult + 14, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution 2 scan memory address.");
			return;
		}

		std::uint8_t* Resolution3ScanResult = Memory::PatternScan(exeModule, "68 E0 01 00 00 68 80 02 00 00 E8 C0 29 22 00 83 C4 0C C7 44 24 0C 38 E8 AE 00 89 5C 24 10 C7 44 24 14 E0 01 00 00 89 5C 24 18 C7 44 24 1C 80 02 00 00 8B 15 E0 AB BC 00 8B 42 24 8B 48 04 8B 01 8D 54 24 0C 52 89 5C 24 48 FF 50 50 A1 24 97 BC 00 8B 70 64 3B F3 74 3A 57 8B CE E8 8F 21 21 00 8B 78 60 8B CE E8 85 21 21 00 3B FB 8B 40 64 74 0E C7 47 0C 80 02 00 00 C7 47 10 E0 01 00 00 3B C3 5F 74 0E C7 40 0C 80 02 00 00 C7 40 10 E0 01 00");
		if (Resolution3ScanResult)
		{
			spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution3ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(Resolution3ScanResult + 1, iCurrentResY);
			
			Memory::Write(Resolution3ScanResult + 6, iCurrentResX);
			
			Memory::Write(Resolution3ScanResult + 34, iCurrentResY);
			
			Memory::Write(Resolution3ScanResult + 46, iCurrentResX);

			Memory::Write(Resolution3ScanResult + 116, iCurrentResX);

			Memory::Write(Resolution3ScanResult + 123, iCurrentResY);

			Memory::Write(Resolution3ScanResult + 135, iCurrentResX);

			Memory::Write(Resolution3ScanResult + 142, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution 3 scan memory address.");
			return;
		}

		std::uint8_t* Resolution4ScanResult = Memory::PatternScan(exeModule, "8B 0F 89 0D 60 9B C2 00 8B 57 04 89 15 64 9B C2 00");
		if (Resolution4ScanResult)
		{
			spdlog::info("Resolution 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution4ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstructionMidHook{};

			ResolutionWidthInstructionMidHook = safetyhook::create_mid(Resolution4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi) = iCurrentResX;
			});

			static SafetyHookMid ResolutionHeightInstructionMidHook{};

			ResolutionHeightInstructionMidHook = safetyhook::create_mid(Resolution4ScanResult + 8, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi + 0x4) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution 4 scan memory address.");
			return;
		}

		std::uint8_t* Resolution5ScanResult = Memory::PatternScan(exeModule, "8B 81 4C 01 00 00 C3 90 90 90 90 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC A1 24 97 BC 00 8A 50 70 84 D2 74 06 B8 E0 01 00 00 C3 8B 49 10 8B 81 50 01 00 00 C3 90 90 90 90");
		if (Resolution5ScanResult)
		{
			spdlog::info("Resolution 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution5ScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid Resolution5WidthInstructionMidHook{};
			
			Resolution5WidthInstructionMidHook = safetyhook::create_mid(Resolution5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x14C) = iCurrentResX;
			});

			static SafetyHookMid Resolution5HeightInstructionMidHook{};
			
			Resolution5HeightInstructionMidHook = safetyhook::create_mid(Resolution5ScanResult + 48, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x150) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution 5 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 4E 68 8B 50 04 D8 76 68 89 56 6C");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

				if (fCurrentCameraHFOV == 0.5f)
				{
					fNewCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV) * fFOVFactor;
				}
				else if (fCurrentCameraHFOV != 631.959716797f)
				{
					fNewCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV);
				}
				else
				{
					fNewCameraHFOV = fCurrentCameraHFOV;
				}

				*reinterpret_cast<float*>(ctx.esi + 0x68) = fNewCameraHFOV;
			});

			Memory::PatchBytes(CameraFOVInstructionScanResult + 9, "\x90\x90\x90", 3);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 9, [](SafetyHookContext& ctx)
			{
				fNewCameraVFOV = fNewCameraHFOV / fNewAspectRatio;

				*reinterpret_cast<float*>(ctx.esi + 0x6C) = fNewCameraVFOV;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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