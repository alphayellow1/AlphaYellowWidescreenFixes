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
std::string sFixName = "TennisMastersSeries2003FOVFix";
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

// Constants
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;

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
	TMS2003,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TMS2003, {"Tennis Masters Series 2003", "Tennis Masters Series 2003.exe"}},
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
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));
}

void FOVFix()
{
	if (eGameType == Game::TMS2003 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* Camera5and6FOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 84 8E C0 00 00 00 D8 0D ?? ?? ?? ?? 8B 4E 14 89 44 24 1C 89 54 24 18 8D 44 24 08");
		if (Camera5and6FOVInstructionScanResult)
		{
			spdlog::info("Camera 5 & 6 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), Camera5and6FOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid Camera5and6FOVInstructionMidHook{};

			static std::vector<float> vComputedCamera5And6FOVs;

			Camera5and6FOVInstructionMidHook = safetyhook::create_mid(Camera5and6FOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCamera5And6FOV = *reinterpret_cast<float*>(ctx.esi + ctx.ecx * 0x4 + 0xC0);

				// Checks if this FOV has already been computed
				if (std::find(vComputedCamera5And6FOVs.begin(), vComputedCamera5And6FOVs.end(), fCurrentCamera5And6FOV) != vComputedCamera5And6FOVs.end())
				{
					// Value already processed, then skips the calculations
					return;
				}

				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fCurrentCamera5And6FOV = CalculateNewFOV(fCurrentCamera5And6FOV) * fFOVFactor;

				// Stores the new value so future calls can skip re-calculations
				vComputedCamera5And6FOVs.push_back(fCurrentCamera5And6FOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera 5 & 6 FOV instruction memory address.");
			return;
		}

		std::uint8_t* Camera7FOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 86 AC 00 00 00 D8 0D ?? ?? ?? ?? 8B 86 B4 00 00 00 D8 0D ?? ?? ?? ??");
		if (Camera7FOVInstructionScanResult)
		{
			spdlog::info("Camera 7 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), Camera7FOVInstructionScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid Camera7FOVInstructionMidHook{};
			
			static std::vector<float> vComputedCamera7FOVs;
			
			Camera7FOVInstructionMidHook = safetyhook::create_mid(Camera7FOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCamera7FOV = *reinterpret_cast<float*>(ctx.esi + 0xAC);
					
				// Checks if this FOV has already been computed
				if (std::find(vComputedCamera7FOVs.begin(), vComputedCamera7FOVs.end(), fCurrentCamera7FOV) != vComputedCamera7FOVs.end())
				{
					// Value already processed, then skips the calculations
					return;
				}
					
				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fCurrentCamera7FOV = CalculateNewFOV(fCurrentCamera7FOV) * fFOVFactor;
					
				// Stores the new value so future calls can skip re-calculations
				vComputedCamera7FOVs.push_back(fCurrentCamera7FOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera 7 FOV instruction memory address.");
			return;
		}

		std::uint8_t* Camera1To4FOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 86 AC 00 00 00 D8 0D ?? ?? ?? ?? 8B 8E B4 00 00 00 8B 86 B0 00 00 00");
		if (Camera1To4FOVInstructionScanResult)
		{
			spdlog::info("Camera 1 to 4 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), Camera1To4FOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid Camera1To4FOVInstructionMidHook{};

			static std::vector<float> vComputedCamera1To4FOVs;

			Camera1To4FOVInstructionMidHook = safetyhook::create_mid(Camera1To4FOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCamera1To4FOV = *reinterpret_cast<float*>(ctx.esi + 0xAC);

				// Checks if this FOV has already been computed
				if (std::find(vComputedCamera1To4FOVs.begin(), vComputedCamera1To4FOVs.end(), fCurrentCamera1To4FOV) != vComputedCamera1To4FOVs.end())
				{
					// Value already processed, then skips the calculations
					return;
				}

				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fCurrentCamera1To4FOV = CalculateNewFOV(fCurrentCamera1To4FOV) * fFOVFactor;

				// Stores the new value so future calls can skip re-calculations
				vComputedCamera1To4FOVs.push_back(fCurrentCamera1To4FOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera 1 to 4 FOV instruction memory address.");
			return;
		}

		std::uint8_t* CutscenesCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 86 9C 00 00 00 D8 A6 98 00 00 00 8B 86 A0 00 00 00 DE C9 D8 86 98 00 00 00");
		if (CutscenesCameraFOVInstructionScanResult)
		{
			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CutscenesCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CutscenesCameraHFOVInstructionMidHook{};

			static std::vector<float> vComputedFOVs6;

			CutscenesCameraHFOVInstructionMidHook = safetyhook::create_mid(CutscenesCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV6 = *reinterpret_cast<float*>(ctx.esi + 0x9C);

				// Checks if this FOV has already been computed
				if (std::find(vComputedFOVs6.begin(), vComputedFOVs6.end(), fCurrentCameraFOV6) != vComputedFOVs6.end())
				{
					// Value already processed, then skips the calculations
					return;
				}

				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fCurrentCameraFOV6 = CalculateNewFOV(fCurrentCameraFOV6);

				// Stores the new value so future calls can skip re-calculations
				vComputedFOVs6.push_back(fCurrentCameraFOV6);
			});
		}
		else
		{
			spdlog::info("Cannot locate the cutscenes camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction1ScanResult = Memory::PatternScan(exeModule, "D8 70 1C 8B 86 B0 00 00 00 D9 C1 D9 E0 D9 5C 24 04 D9 C1 D9 5C 24 08");
		if (AspectRatioInstruction1ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction1ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid AspectRatioInstructionMidHook{};

			AspectRatioInstructionMidHook = safetyhook::create_mid(AspectRatioInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.eax + 0x1C) = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(exeModule, "D8 70 1C D9 C1 D9 E0 D9 5C 24 04 D9 C1 D9 5C 24 08 DE C9 D9 54 24 0C D9 E0");
		if (AspectRatioInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid AspectRatioInstruction2MidHook{};
			
			AspectRatioInstruction2MidHook = safetyhook::create_mid(AspectRatioInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.eax + 0x1C) = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction3ScanResult = Memory::PatternScan(exeModule, "D8 71 1C 8B 8E 88 00 00 00 D9 C1 D9 E0 D9 5C 24 0C D9 C1 D9 5C 24 10 DE C9 D9 54 24 14 D9 E0 D9 5C 24 18 8B 50 5C 89 54 24 1C");
		if (AspectRatioInstruction3ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction3ScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid AspectRatioInstruction3MidHook{};
			
			AspectRatioInstruction3MidHook = safetyhook::create_mid(AspectRatioInstruction3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.eax + 0x1C) = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction 3 memory address.");
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