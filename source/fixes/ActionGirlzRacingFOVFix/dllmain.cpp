﻿// Include necessary headers
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
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "ActionGirlzRacingFOVFix";
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
constexpr float fTolerance = 0.00001f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fFOVFactor;
float fAspectRatioScale;
static float fCurrentCameraFOV;

// Game detection
enum class Game
{
	AGR,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AGR, {"Action Girlz Racing", "Action Girlz Racing.exe"}},
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

void FOVFix()
{
	if (eGameType == Game::AGR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 46 18 D9 F2 66 8B 46 10 66 8B 4E 12 89 44 24 10");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x18);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraHFOVAndVFOVInstructionsScanResult = Memory::PatternScan(exeModule, "89 4E 68 8B 50 04 D8 76 68 89 56 6C 8B 46 04 85 C0");
		if (CameraHFOVAndVFOVInstructionsScanResult)
		{
			spdlog::info("Camera HFOV and FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVAndVFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVAndVFOVInstructionsScanResult, "\x90\x90\x90", 3); // NOP out the original instruction

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVAndVFOVInstructionsScanResult + 3, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

				if (fabsf(fCurrentCameraFOV - 0.8999999762f) < fTolerance)
				{
					fNewCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV) * fFOVFactor; // Apply FOV factor if the current camera FOV is 0.9
				}
				else
				{
					fNewCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV);
				}

				spdlog::debug(" → new HFOV = {}", fNewCameraHFOV);

				*reinterpret_cast<float*>(ctx.esi + 0x68) = fNewCameraHFOV;
			});

			Memory::PatchBytes(CameraHFOVAndVFOVInstructionsScanResult + 9, "\x90\x90\x90", 3); // NOP out the original instruction

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVAndVFOVInstructionsScanResult + 12, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraVFOV = std::bit_cast<float>(ctx.edx);

				if (fabsf(fCurrentCameraFOV - 0.8999999762f) < fTolerance)
				{
					fNewCameraVFOV = fCurrentCameraVFOV * fFOVFactor; // Apply FOV factor if the current camera FOV is 0.9
				}
				else
				{
					fNewCameraVFOV = fCurrentCameraVFOV;
				}

				spdlog::debug("Hook3 fired: raw VFOV = {} → new VFOV = {}", fCurrentCameraVFOV, fNewCameraVFOV);

				*reinterpret_cast<float*>(ctx.esi + 0x6C) = fNewCameraVFOV;
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV and VFOV instructions scan memory address.");
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