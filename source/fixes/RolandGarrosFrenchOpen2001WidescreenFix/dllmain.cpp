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
HMODULE dllModule2;

// Fix details
std::string sFixName = "RolandGarrosFrenchOpen2001WidescreenFix";
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
int iNewMenuResX;
int iNewMenuResY;
int iNewMatchResX;
int iNewMatchResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
uint8_t* CameraHFOVAddress;
uint8_t* CameraVFOVAddress;
float fNewCameraHFOV;
float fNewCameraVFOV;

// Game detection
enum class Game
{
	RGFO2001,
	Unknown
};

enum MenuResolutionInstructionsScan
{
	MenuRes1,
	MenuRes2,
	MenuRes3,
	MenuRes4,
	MenuRes5
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RGFO2001, {"Roland Garros French Open 2001", "RG2001.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "MenuResolutionWidth", iNewMenuResX);
	inipp::get_value(ini.sections["Settings"], "MenuResolutionHeight", iNewMenuResY);
	inipp::get_value(ini.sections["Settings"], "MatchResolutionWidth", iNewMatchResX);
	inipp::get_value(ini.sections["Settings"], "MatchResolutionHeight", iNewMatchResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iNewMenuResX);
	spdlog_confparse(iNewMenuResY);
	spdlog_confparse(iNewMatchResX);
	spdlog_confparse(iNewMatchResY);
	spdlog_confparse(fFOVFactor);

	// If resolution not specified, use desktop resolution
	if ((iNewMatchResX <= 0 || iNewMatchResY <= 0) || (iNewMenuResX <= 0 || iNewMenuResY <= 0))
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();

		if (iNewMenuResX <= 0 || iNewMenuResY <= 0)
		{
			iNewMenuResX = desktopDimensions.first;
			iNewMenuResY = desktopDimensions.second;
			spdlog_confparse(iNewMenuResX);
			spdlog_confparse(iNewMenuResY);
		}
		else if (iNewMatchResX <= 0 || iNewMatchResY <= 0)
		{
			iNewMatchResX = desktopDimensions.first;
			iNewMatchResY = desktopDimensions.second;
			spdlog_confparse(iNewMatchResX);
			spdlog_confparse(iNewMatchResY);
		}
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

	dllModule2 = Memory::GetHandle("rcMain.dll");

	return true;
}

static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::RGFO2001 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iNewMatchResX) / static_cast<float>(iNewMatchResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> MenuResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF D5 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF D7 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 15", "81 38 ?? ?? ?? ?? 0F 95 ?? 84 C0 74 ?? 8B 0D");
		if (Memory::AreAllSignaturesValid(MenuResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuRes1] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuRes2] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuRes3] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuRes4] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuResolutionInstructionsScansResult[MenuRes5] - (std::uint8_t*)exeModule);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes1] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes1] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes2] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes2] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes3] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes3] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes4] + 6, iNewMenuResX);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes4] + 1, iNewMenuResY);

			Memory::Write(MenuResolutionInstructionsScansResult[MenuRes5] + 2, iNewMenuResX);
		}

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "66 C7 41 ?? ?? ?? 66 C7 41 ?? ?? ?? C2 ?? ?? 66 C7 41 ?? ?? ?? 66 C7 41 ?? ?? ?? C2 ?? ?? 66 C7 41 ?? ?? ?? 66 C7 41 ?? ?? ?? C2 ?? ?? 66 C7 41 ?? ?? ?? 66 C7 41 ?? ?? ?? C2 ?? ?? 66 C7 41 ?? ?? ?? 66 C7 41 ?? ?? ?? C2 ?? ?? 66 C7 41 ?? ?? ?? 66 C7 41 ?? ?? ?? C2 ?? ?? 66 C7 41 ?? ?? ?? 66 C7 41 ?? ?? ?? C2 ?? ?? 66 C7 41");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionListScanResult + 4, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 10, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 19, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 25, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 34, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 40, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 49, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 55, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 64, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 70, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 79, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 85, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 94, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 100, (uint16_t)iNewMatchResY);

			Memory::Write(ResolutionListScanResult + 109, (uint16_t)iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 115, (uint16_t)iNewMatchResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list memory address.");
			return;
		}		

		// Located in rcMain.Runn::RCamera::think
		std::uint8_t* CameraFOVInstructionsScanResult = Memory::PatternScan(dllModule2, "D8 0D ?? ?? ?? ?? D9 9E ?? ?? ?? ?? DB 05 ?? ?? ?? ?? D8 C9 D8 0D ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 5E");
		if (CameraFOVInstructionsScanResult)
		{
			spdlog::info("Camera HFOV Instruction Scan: Address is rcMain.dll+{:x}", CameraFOVInstructionsScanResult - (std::uint8_t*)dllModule2);

			spdlog::info("Camera VFOV Instruction Scan: Address is rcMain.dll+{:x}", CameraFOVInstructionsScanResult + 20 - (std::uint8_t*)dllModule2);

			CameraHFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScanResult + 2, Memory::PointerMode::Absolute);

			CameraVFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScanResult + 22, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScanResult, 6);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(CameraHFOVAddress);

				fNewCameraHFOV = (fCurrentCameraHFOV / fAspectRatioScale) / fFOVFactor;

				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult + 20, 6);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult + 20, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(CameraVFOVAddress);

				fNewCameraVFOV = fCurrentCameraVFOV / fFOVFactor;

				FPU::FMUL(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instructions scan memory address.");
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