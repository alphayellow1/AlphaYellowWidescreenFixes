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
std::string sFixName = "BackyardHockey2005WidescreenFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewHUDHorizontalRes;

// Game detection
enum class Game
{
	BH2005_DX9,
	BH2005_DX8,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BH2005_DX9, {"Backyard Hockey 2005 (DX9)", "Hockey2005.exe"}},
	{Game::BH2005_DX8, {"Backyard Hockey 2005 (DX8)", "Hockey2005DX8.exe"}},
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

static SafetyHookMid ResolutionWidth640Hook{};
static SafetyHookMid ResolutionHeight480Hook{};
static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid HUDHorizontalResInstructionHook{};

void WidescreenFix()
{
	if ((eGameType == Game::BH2005_DX9 || eGameType == Game::BH2005_DX8) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "89 44 24 ?? E8 ?? ?? ?? ?? 8B CB");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 640x480
			Memory::PatchBytes(ResolutionListScanResult, "\x90\x90\x90\x90", 4);

			ResolutionWidth640Hook = safetyhook::create_mid(ResolutionListScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esp + 0x14) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionListScanResult + 11, "\x90\x90\x90\x90", 4);

			ResolutionHeight480Hook = safetyhook::create_mid(ResolutionListScanResult + 11, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esp + 0x18) = iCurrentResY;
			});

			// 1024x768
			Memory::Write(ResolutionListScanResult + 41, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 49, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 59, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 67, iCurrentResY);
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 46 ?? 83 C4 ?? 68");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x2C);

				fNewCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* HUDHorizontalResInstructionScanResult = Memory::PatternScan(exeModule, "8B 44 24 ?? 8B 4C 24 ?? 89 04 ?? 8B 44 24 ?? 8D 14");
		if (HUDHorizontalResInstructionScanResult)
		{
			spdlog::info("HUD Horizontal Res Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDHorizontalResInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(HUDHorizontalResInstructionScanResult, "\x90\x90\x90\x90", 4);

			HUDHorizontalResInstructionHook = safetyhook::create_mid(HUDHorizontalResInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDVerticalRes = *reinterpret_cast<float*>(ctx.esp + 0x14);

				fNewHUDHorizontalRes = fCurrentHUDVerticalRes * fNewAspectRatio;

				ctx.eax = std::bit_cast<uintptr_t>(fNewHUDHorizontalRes);
			});
		}
		else
		{
			spdlog::error("Failed to locate HUD horizontal resolution instruction memory address.");
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