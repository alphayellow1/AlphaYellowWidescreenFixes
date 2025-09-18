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
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "ArmyMenSargesWarFOVFix";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewCameraHFOV;
float fNewCameraHFOV2;
float fNewCameraVFOV;
float fNewCameraVFOV2;
float fNewAspectRatio;
float fAspectRatioScale;

// Game detection
enum class Game
{
	AMSW,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AMSW, {"Army Men: Sarge's War", "amsw.exe"}},
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
	GetModuleFileNameW(dllModule, exePathW, MAX_PATH);
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

void FOVFix()
{
	if (eGameType == Game::AMSW && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 48 68 D9 5C 24 0C D9 05 ?? ?? ?? ?? D8 70 68 8B 54 24 0C 89 50 6C D9 58 70 D9 05 ?? ?? ?? ?? D8 70 6C D9 58 74 8B 40 04 85 C0 74 09 50 E8 ?? ?? ?? ?? 83 C4 04 46 3B F7 0F 8E ?? ?? ?? ?? 5F 5E 83 C4 08 C3 CC CC 83 EC 10 89 04 24 8B 44 24 14 89 44 24 0C");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

				if (fCurrentCameraHFOV == 0.5773502588f)
				{
					fNewCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale);
				}

				*reinterpret_cast<float*>(ctx.eax + 0x68) = fNewCameraHFOV;
			});

			Memory::PatchBytes(CameraFOVInstructionScanResult + 20, "\x90\x90\x90", 3);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 20, [](SafetyHookContext& ctx)
			{
				fNewCameraVFOV = fNewCameraHFOV / fNewAspectRatio;

				*reinterpret_cast<float*>(ctx.eax + 0x6C) = fNewCameraVFOV;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "89 48 68 D9 5C 24 0C D9 05 ?? ?? ?? ?? D8 70 68 8B 54 24 0C 89 50 6C D9 58 70 D9 05 ?? ?? ?? ?? D8 70 6C D9 58 74 8B 40 04 85 C0 74 09 50 E8 ?? ?? ?? ?? 83 C4 04 46 3B F7 0F 8E ?? ?? ?? ?? 5F 5E 83 C4 08 C3 CC CC D9 44 24 04 83 EC 08 D8 1D ?? ?? ?? ?? 56 8B F0");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid CameraHFOVInstruction2MidHook{};

			CameraHFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraHFOV2 = std::bit_cast<float>(ctx.ecx);

				if (fCurrentCameraHFOV2 == 0.7002075315f)
				{
					fNewCameraHFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV2, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewCameraHFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV2, fAspectRatioScale);
				}

				*reinterpret_cast<float*>(ctx.eax + 0x68) = fNewCameraHFOV2;
			});

			Memory::PatchBytes(CameraFOVInstruction2ScanResult + 20, "\x90\x90\x90", 3);

			static SafetyHookMid CameraVFOVInstruction2MidHook{};

			CameraVFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 20, [](SafetyHookContext& ctx)
			{
				fNewCameraVFOV2 = fNewCameraHFOV2 / fNewAspectRatio;

				*reinterpret_cast<float*>(ctx.eax + 0x6C) = fNewCameraVFOV2;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
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