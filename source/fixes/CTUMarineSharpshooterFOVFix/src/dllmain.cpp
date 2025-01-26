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
std::string sFixName = "CTUMarineSharpshooterFOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float epsilon = 0.00001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;

// Game detection
enum class Game
{
	CTUMS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CTUMS, {"CTU Marine Sharpshooter", "lithtech.exe"}},
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
		}
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}

	if (!dllModule2)
	{
		dllModule2 = GetModuleHandleA("cshell.dll");
		spdlog::info("Waiting for cshell.dll to load...");
		Sleep(1000);
	}

	return true;
}

void FOVFix()
{
	if (eGameType == Game::CTUMS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 98 01 00 00 89 45 BC");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.04719758f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.046400428f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(1.04719758f / 2.0f) * (fNewAspectRatio / fOldAspectRatio));
				}
			});
		}

		std::uint8_t* CameraHFOVZoomInstructionScanResult = Memory::PatternScan(exeModule, "89 81 98 01 00 00 8B 45 10");
		if (CameraHFOVZoomInstructionScanResult)
		{
			spdlog::info("Camera HFOV Zoom Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVZoomInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVZoomInstructionMidHook{};

			CameraHFOVZoomInstructionMidHook = safetyhook::create_mid(CameraHFOVZoomInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (ctx.eax > std::bit_cast<uint32_t>(0.51f) && ctx.eax < std::bit_cast<uint32_t>(0.53f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(0.5235988498f / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
				}
			});
		}

		/*
		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 B4 24 D0 00 00 00 8B B0 54 01 00 00 89 B4 24 D4 00 00 00");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult + 0x7 - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};
			static float lastModifiedVFOV = 0.0f; // Tracks the last value modified by the hook

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult + 0x7, [](SafetyHookContext& ctx)
			{
				float& currentVFOVValue = *reinterpret_cast<float*>(ctx.eax + 0x154);

				// Handle specific initialization cases
				if (fabs(currentVFOVValue - (0.8835729361f / (fNewAspectRatio / 1.33150439411f))) < epsilon)
				{
					currentVFOVValue = 0.8835429368f;
					lastModifiedVFOV = currentVFOVValue; // Update tracking variable
					return;
				}

				if (fabs(currentVFOVValue - (0.7853981256f / (fNewAspectRatio / fOldAspectRatio))) < epsilon)
				{
					currentVFOVValue = 0.7853981256f;
					lastModifiedVFOV = currentVFOVValue; // Update tracking variable
					return;
				}

				// Avoid recursive modifications
				if (currentVFOVValue != lastModifiedVFOV &&
					currentVFOVValue != 0.8835429368f &&
					currentVFOVValue != 0.7853981256f &&
					currentVFOVValue != 0.2293362767f)
				{
					float modifiedVFOVValue = currentVFOVValue / (fOldAspectRatio / fNewAspectRatio);

					// Only modify if the calculated value differs
					if (currentVFOVValue != modifiedVFOVValue)
					{
						currentVFOVValue = modifiedVFOVValue;
						lastModifiedVFOV = modifiedVFOVValue; // Update tracking variable
					}
				}
			});
		}
		*/
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