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
std::string sFixName = "VietnamBlackOpsFOVFix";
std::string sFixVersion = "1.4";
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
	VBO,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::VBO, {"Vietnam: Black Ops", "BlackOps.exe"}},
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
			return true;
		}
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}
}

void FOVFix()
{
	if (eGameType == Game::VBO && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B B0 38 01 00 00 89 B4 24 D0 00 00 00");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax + 0x138);

				if (fCurrentCameraHFOV == 1.570796371f)
				{
					fCurrentCameraHFOV = 2.0f * atanf(tanf(fCurrentCameraHFOV / 2.0f) * (fNewAspectRatio / fOldAspectRatio));
				}
			});
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B B0 3C 01 00 00 89 B4 24 D4 00 00 00");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x13C);

				if (fabs(fCurrentCameraVFOV - (1.178097248f / (fNewAspectRatio / fOldAspectRatio))) < epsilon)
				{
					fCurrentCameraVFOV = 1.178097248f;
				}
				else if (fabs(fCurrentCameraVFOV - (0.13089969754219055f / (fNewAspectRatio / fOldAspectRatio))) < epsilon)
				{
					fCurrentCameraVFOV = 0.13089969754219055f;
				}
			});
		}

		std::uint8_t* CameraHFOVZoomInstructionScanResult = Memory::PatternScan(exeModule, "89 81 38 01 00 00 C3 90 90 90 90 90 90");
		if (CameraHFOVZoomInstructionScanResult)
		{
			spdlog::info("Camera HFOV Zoom Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVZoomInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVZoomInstructionMidHook{};

			CameraHFOVZoomInstructionMidHook = safetyhook::create_mid(CameraHFOVZoomInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (ctx.eax == std::bit_cast<uint32_t>(0.174532935f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(0.174532935f / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
				}
			});
		}

		std::uint8_t* CameraVFOVZoomInstructionScanResult = Memory::PatternScan(exeModule, "8B 44 24 08 D9 99 3C 01 00 00");
		if (CameraVFOVZoomInstructionScanResult)
		{
			spdlog::info("Camera VFOV Zoom Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVZoomInstructionScanResult + 0x4 - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVZoomInstructionMidHook{};

			CameraVFOVZoomInstructionMidHook = safetyhook::create_mid(CameraVFOVZoomInstructionScanResult + 0x4, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOVZoom = *reinterpret_cast<float*>(ctx.ecx + 0x13C);

				if (fabs(fCurrentCameraVFOVZoom - (0.13089969754219055f / (fNewAspectRatio / fOldAspectRatio))) < epsilon)
				{
					fCurrentCameraVFOVZoom = 0.13089969754219055f;
				}
			});
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