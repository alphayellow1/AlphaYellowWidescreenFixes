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
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "18WheelsOfSteelHaulinFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewMainMenuCameraFOV;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
	return degrees * (fPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
	return radians * (180.0f / fPi);
}

// Game detection
enum class Game
{
	WOSH,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WOSH, {"18 Wheels of Steel: Haulin'", "prism3d.exe"}},
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
		}
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}

	while (!dllModule2)
	{
		dllModule2 = GetModuleHandleA("p3core.dll");
		spdlog::info("Waiting for p3core.dll to load...");
		Sleep(1000);
	}

	spdlog::info("Successfully obtained handle for p3core.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while (!dllModule3)
	{
		dllModule3 = GetModuleHandleA("game.dll");
		spdlog::info("Waiting for game.dll to load...");
		Sleep(1000);
	}

	spdlog::info("Successfully obtained handle for game.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	return true;
}

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));
}

void FOVFix()
{
	if (eGameType == Game::WOSH && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* MainMenuCameraFOVScanResult = Memory::PatternScan(dllModule3, "C7 44 24 74 00 00 48 42 8D 54 24 54 55 52 C6 84 24 94 00 00 00 00");
		if (MainMenuCameraFOVScanResult)
		{
			spdlog::info("Main Menu Camera FOV: Address is game.dll+{:x}", MainMenuCameraFOVScanResult + 0x4 - (std::uint8_t*)dllModule3);

			fNewMainMenuCameraFOV = CalculateNewFOV(50.0f);

			Memory::Write(MainMenuCameraFOVScanResult + 0x4, fNewMainMenuCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate main menu camera FOV memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(dllModule2, "D9 44 24 14 8B 44 24 04 D8 0D ?? ?? ?? ??");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is p3core.dll+{:x}", CameraFOVInstruction1ScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid CameraFOVInstruction1MidHook{};

			CameraFOVInstruction1MidHook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOV1Value = *reinterpret_cast<float*>(ctx.esp + 0x14);

				if (fCurrentFOV1Value == 57.0f)
				{
					fCurrentFOV1Value = fFOVFactor * CalculateNewFOV(fCurrentFOV1Value);
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule3, "8B 74 24 08 8B 46 4C 8B 4E 20 8B 56 1C");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is game.dll+{:x}", CameraFOVInstruction2ScanResult + 0x7 - (std::uint8_t*)dllModule3);

			static SafetyHookMid CameraFOVInstruction2MidHook{};

			CameraFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 0x7, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOV2Value = *reinterpret_cast<float*>(ctx.esi + 0x20);

				if (fCurrentFOV2Value == 57.0f)
				{
					fCurrentFOV2Value = fFOVFactor * CalculateNewFOV(fCurrentFOV2Value);
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(dllModule3, "8B 56 4C 8B 46 20 8B 4E 1C");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is game.dll+{:x}", CameraFOVInstruction3ScanResult + 0x3 - (std::uint8_t*)dllModule3);

			static SafetyHookMid CameraFOVInstruction3MidHook{};

			CameraFOVInstruction3MidHook = safetyhook::create_mid(CameraFOVInstruction3ScanResult + 0x3, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOV3Value = *reinterpret_cast<float*>(ctx.esi + 0x20);

				if (fCurrentFOV3Value == 57.0f)
				{
					fCurrentFOV3Value = fFOVFactor * CalculateNewFOV(fCurrentFOV3Value);
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction4ScanResult = Memory::PatternScan(dllModule3, "8B 4E 4C 8B 56 20 8B 46 1C");
		if (CameraFOVInstruction4ScanResult)
		{
			spdlog::info("Camera FOV Instruction 4: Address is game.dll+{:x}", CameraFOVInstruction4ScanResult + 0x3 - (std::uint8_t*)dllModule3);

			static SafetyHookMid CameraFOVInstruction4MidHook{};

			CameraFOVInstruction4MidHook = safetyhook::create_mid(CameraFOVInstruction4ScanResult + 0x3, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOV4Value = *reinterpret_cast<float*>(ctx.esi + 0x20);

				if (fCurrentFOV4Value == 57.0f)
				{
					fCurrentFOV4Value = fFOVFactor * CalculateNewFOV(fCurrentFOV4Value);
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction5ScanResult = Memory::PatternScan(dllModule3, "8B 46 4C 8B 4E 20 8B 56 1C 83 C4 48 50 8B 46 18");
		if (CameraFOVInstruction5ScanResult)
		{
			spdlog::info("Camera FOV Instruction 5: Address is game.dll+{:x}", CameraFOVInstruction5ScanResult + 0x3 - (std::uint8_t*)dllModule3);

			static SafetyHookMid CameraFOVInstruction5MidHook{};

			CameraFOVInstruction5MidHook = safetyhook::create_mid(CameraFOVInstruction5ScanResult + 0x3, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOV5Value = *reinterpret_cast<float*>(ctx.esi + 0x20);

				if (fCurrentFOV5Value == 57.0f)
				{
					fCurrentFOV5Value = fFOVFactor * CalculateNewFOV(fCurrentFOV5Value);
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 5 memory address.");
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