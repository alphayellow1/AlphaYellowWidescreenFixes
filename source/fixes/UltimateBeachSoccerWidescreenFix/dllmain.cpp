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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "UltimateBeachSoccerWidescreenFix";
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
float fAspectRatioScale;
float fFOVFactor;
float fNewCameraHFOV;
float fNewCameraVFOV;

// Game detection
enum class Game
{
	UBSCONFIG,
	UBSGAME,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::UBSCONFIG, {"Ultimate Beach Soccer - Configuration", "Resolution.exe"}},
	{Game::UBSGAME, {"Ultimate Beach Soccer", "BeachSoccer.exe"}},
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
	if (bFixActive == true)
	{
		if (eGameType == Game::UBSCONFIG)
		{
			std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "81 7D E8 80 02 00 00 75 09 81 7D EC E0 01 00 00 74 36 81 7D E8 20 03 00 00 75 09 81 7D EC 58 02 00 00 74 24 81 7D E8 00 04 00 00 75 09 81 7D EC 00 03 00 00 74 12 81 7D E8 00 05 00 00 75 38 81 7D EC 00 04 00 00");
			if (ResolutionListScanResult)
			{
				spdlog::info("Resolution List: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

				// 640x480
				Memory::Write(ResolutionListScanResult + 3, iCurrentResX);

				Memory::Write(ResolutionListScanResult + 12, iCurrentResY);

				// 800x600
				Memory::Write(ResolutionListScanResult + 21, iCurrentResX);

				Memory::Write(ResolutionListScanResult + 30, iCurrentResY);

				// 1024x768
				Memory::Write(ResolutionListScanResult + 39, iCurrentResX);

				Memory::Write(ResolutionListScanResult + 48, iCurrentResY);

				// 1280x1024
				Memory::Write(ResolutionListScanResult + 57, iCurrentResX);

				Memory::Write(ResolutionListScanResult + 66, iCurrentResY);
			}
			else
			{
				spdlog::error("Failed to locate resolution list memory address.");
				return;
			}
		}

		if (eGameType == Game::UBSGAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 4E 68 8B 50 04 D8 76 68 89 56 6C 8B 46 04 85 C0 D9 5E 70");
			if (CameraFOVInstructionScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

				static SafetyHookMid CameraHFOVInstructionMidHook{};

				CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

					if (fCurrentCameraHFOV == 0.2015136331f || fCurrentCameraHFOV == 0.4867046773f || fCurrentCameraHFOV == 0.2015138566f || fCurrentCameraHFOV == 0.2357445806f || fCurrentCameraHFOV == 0.235744819f)
					{
						fNewCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV) * fFOVFactor;
					}
					else
					{
						fNewCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV);
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