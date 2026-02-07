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
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "CoronelIndoorKartracingWidescreenFix";
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
float fNewGameplayCameraFOV;
float fNewMenuCameraFOV;
float fNewAspectRatio;
float fFOVFactor;
float fAspectRatioScale;

// Game detection
enum class Game
{
	CIK,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CIK, {"Coronel Indoor Kartracing", "kart.exe"}},
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
	bool bGameFound = false;

	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			bGameFound = true;
			break;
		}
	}

	if (bGameFound == false)
	{
		spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		return false;
	}

	dllModule2 = Memory::GetHandle("qserver.dll");

	dllModule3 = Memory::GetHandle("world.dll");

	return true;
}

static SafetyHookMid MenuCameraFOVInstructionHook{};
static SafetyHookMid GameplayCameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::CIK && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 80 04 00 00 60 03 00 00 00 05 00 00 C0 03 00 00 00 05 00 00 00 04 00 00 40 06 00 00 B0 04 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 640x480
			Memory::Write(ResolutionListScanResult, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 4, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 12, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 16, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 20, iCurrentResY);

			// 1152x864
			Memory::Write(ResolutionListScanResult + 24, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 28, iCurrentResY);

			// 1280x960
			Memory::Write(ResolutionListScanResult + 32, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 36, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 40, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 44, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionListScanResult + 48, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 52, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list memory address.");
			return;
		}

		std::uint8_t* MenuCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 4C 24 44 8B D8 8B 44 24 48 D9 5C 24 34 D9 44 24 58 D9 5C 24 38 50 D9 44 24 54 51");
		if (MenuCameraFOVInstructionScanResult)
		{
			spdlog::info("Menu Camera FOV Instruction: Address is qserver.dll+{:x}", MenuCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(MenuCameraFOVInstructionScanResult, 4);			

			MenuCameraFOVInstructionHook = safetyhook::create_mid(MenuCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x44);

				fNewMenuCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentMenuCameraFOV, 1.0f / fAspectRatioScale);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewMenuCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the menu camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* GameplayCameraFOVInstructionScanResult = Memory::PatternScan(dllModule3, "D9 00 DC C0 DE F9 D9 58 24 C3 90 90 90 90 90");
		if (GameplayCameraFOVInstructionScanResult)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is world.dll+{:x}", GameplayCameraFOVInstructionScanResult - (std::uint8_t*)dllModule3);

			GameplayCameraFOVInstructionHook = safetyhook::create_mid(GameplayCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentGameplayCameraFOV = *reinterpret_cast<float*>(ctx.eax);

				fNewGameplayCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentGameplayCameraFOV, 1.0f / fAspectRatioScale) / fFOVFactor;

				*reinterpret_cast<float*>(ctx.eax) = fNewGameplayCameraFOV;
			});
		}
		else
		{
			spdlog::info("Cannot locate the gameplay camera FOV instruction memory address.");
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