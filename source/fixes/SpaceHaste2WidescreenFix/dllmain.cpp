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
std::string sFixName = "SpaceHaste2WidescreenFix";
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

// Game detection
enum class Game
{
	SH2,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SH2, {"Space Haste 2", "SpaceHaste.exe"}},
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
	return fCurrentFOV * (fNewAspectRatio / fOldAspectRatio);
}

void WidescreenFix()
{
	if (eGameType == Game::SH2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* Resolution512x384ScanResult = Memory::PatternScan(exeModule, "50 68 80 01 00 00 68 00 02 00 00 51 8B 0D ?? ?? ?? ??");
		if (Resolution512x384ScanResult)
		{
			spdlog::info("Resolution 512x384: Address is {:s}+{:x}", sExeName.c_str(), Resolution512x384ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution512x384ScanResult + 0x2, iCurrentResY);

			Memory::Write(Resolution512x384ScanResult + 0x7, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 512x384 scan memory address.");
			return;
		}

		std::uint8_t* Resolution512x384Scan2Result = Memory::PatternScan(exeModule, "CF 51 68 80 01 00 00 68 00 02 00 00 52 E9 9A");
		if (Resolution512x384Scan2Result)
		{
			spdlog::info("Resolution 512x384 Scan 2: Address is {:s}+{:x}", sExeName.c_str(), Resolution512x384Scan2Result - (std::uint8_t*)exeModule);

			Memory::Write(Resolution512x384Scan2Result + 0x3, iCurrentResY);

			Memory::Write(Resolution512x384Scan2Result + 0x8, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 512x384 scan 2 memory address.");
			return;
		}

		std::uint8_t* Resolution512x384Scan3Result = Memory::PatternScan(exeModule, "A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 68 80 01 00 00 68 00 02 00 00");
		if (Resolution512x384Scan3Result)
		{
			spdlog::info("Resolution 512x384 Scan 3: Address is {:s}+{:x}", sExeName.c_str(), Resolution512x384Scan3Result - (std::uint8_t*)exeModule);

			Memory::Write(Resolution512x384Scan3Result + 0xD, iCurrentResY);

			Memory::Write(Resolution512x384Scan3Result + 0x12, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 512x384 scan 3 memory address.");
			return;
		}

		std::uint8_t* Resolution512x384Scan4Result = Memory::PatternScan(exeModule, "47 00 51 68 80 01 00 00 68 00 02 00 00 52 EB 76");
		if (Resolution512x384Scan4Result)
		{
			spdlog::info("Resolution 512x384 Scan 4: Address is {:s}+{:x}", sExeName.c_str(), Resolution512x384Scan4Result - (std::uint8_t*)exeModule);

			Memory::Write(Resolution512x384Scan4Result + 0x4, iCurrentResY);

			Memory::Write(Resolution512x384Scan4Result + 0x9, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 512x384 scan 4 memory address.");
			return;
		}

		std::uint8_t* Resolution640x480ScanResult = Memory::PatternScan(exeModule, "50 68 E0 01 00 00 68 80 02 00 00 51 8B 0D ?? ?? ?? ??");
		if (Resolution640x480ScanResult)
		{
			spdlog::info("Resolution 640x480: Address is {:s}+{:x}", sExeName.c_str(), Resolution640x480ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution640x480ScanResult + 0x2, iCurrentResY);

			Memory::Write(Resolution640x480ScanResult + 0x7, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 640x480 scan memory address.");
			return;
		}

		std::uint8_t* Resolution800x600ScanResult = Memory::PatternScan(exeModule, "50 68 58 02 00 00 68 20 03 00 00 51 8B 0D ?? ?? ?? ??");
		if (Resolution800x600ScanResult)
		{
			spdlog::info("Resolution 800x600: Address is {:s}+{:x}", sExeName.c_str(), Resolution800x600ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution800x600ScanResult + 0x2, iCurrentResY);

			Memory::Write(Resolution800x600ScanResult + 0x7, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 800x600 scan memory address.");
			return;
		}

		std::uint8_t* Resolution1024x768ScanResult = Memory::PatternScan(exeModule, "50 68 00 03 00 00 68 00 04 00 00 51 8B 0D ?? ?? ?? ??");
		if (Resolution1024x768ScanResult)
		{
			spdlog::info("Resolution 800x600: Address is {:s}+{:x}", sExeName.c_str(), Resolution1024x768ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution1024x768ScanResult + 0x2, iCurrentResY);

			Memory::Write(Resolution1024x768ScanResult + 0x7, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 1024x768 scan memory address.");
			return;
		}

		std::uint8_t* Resolution1280x1024ScanResult = Memory::PatternScan(exeModule, "50 68 00 04 00 00 68 00 05 00 00 51 8B 0D ?? ?? ?? ??");
		if (Resolution1280x1024ScanResult)
		{
			spdlog::info("Resolution 1280x1024: Address is {:s}+{:x}", sExeName.c_str(), Resolution1280x1024ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution1280x1024ScanResult + 0x2, iCurrentResY);

			Memory::Write(Resolution1280x1024ScanResult + 0x7, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 1280x1024 scan memory address.");
			return;
		}

		std::uint8_t* Resolution1600x1200ScanResult = Memory::PatternScan(exeModule, "50 68 B0 04 00 00 68 40 06 00 00 51 8B 0D ?? ?? ?? ??");
		if (Resolution1600x1200ScanResult)
		{
			spdlog::info("Resolution 1600x1200: Address is {:s}+{:x}", sExeName.c_str(), Resolution1600x1200ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(Resolution1600x1200ScanResult + 0x2, iCurrentResY);

			Memory::Write(Resolution1600x1200ScanResult + 0x7, iCurrentResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution 1600x1200 scan memory address.");
			return;
		}

		while (!dllModule2)
		{
			dllModule2 = GetModuleHandleA("trend.dll");
			spdlog::info("Waiting for trend.dll to load...");
			Sleep(1000);
		}

		spdlog::info("Successfully obtained handle for trend.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "8B 50 04 D8 3D ?? ?? ?? ?? 89 15 ?? ?? ?? ??");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is trend.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid AspectRatioInstructionMidHook{};

			AspectRatioInstructionMidHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.eax + 0x4);

				fCurrentAspectRatio = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "A3 ?? ?? ?? ?? 57 33 C0 B9 10 00 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is trend.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			static float fLastModifiedCameraFOV = 0.0f;

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOVValue = std::bit_cast<float>(ctx.eax);

				if (fCurrentCameraFOVValue != fLastModifiedCameraFOV)
				{
					float fModifiedCameraFOVValue = fFOVFactor * CalculateNewFOV(fCurrentCameraFOVValue);

					if (fCurrentCameraFOVValue != fModifiedCameraFOVValue)
					{
						fCurrentCameraFOVValue = fModifiedCameraFOVValue;
						fLastModifiedCameraFOV = fModifiedCameraFOVValue;
					}
				}

				ctx.eax = std::bit_cast<uintptr_t>(fCurrentCameraFOVValue);
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