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
std::string sFixName = "FroggersAdventuresTheRescueWidescreenFix";
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
uint32_t iCurrentResX;
uint32_t iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraFOV;
float fModifiedHFOVValue;

// Game detection
enum class Game
{
	FATR,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FATR, {"Frogger's Adventures: The Rescue", "FrogADV.exe"}},
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
		spdlog::info("Module Address: 0x{0:X}", (uintptr_t)dllModule);
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

float CalculateNewFOV(float fCurrentCameraHFOV)
{
	return fCurrentCameraHFOV * (fNewAspectRatio / fOldAspectRatio);
}

void WidescreenFix()
{
	if (eGameType == Game::FATR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* RendererResolutionScanResult = Memory::PatternScan(exeModule, "00 6A 20 68 E0 01 00 00 68 80 02 00 00 8B 4D FC 51 E8");
		if (RendererResolutionScanResult)
		{
			spdlog::info("Resolution Width: Address is {:s}+{:x}", sExeName.c_str(), RendererResolutionScanResult + 9 - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Height: Address is {:s}+{:x}", sExeName.c_str(), RendererResolutionScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(RendererResolutionScanResult + 9, iCurrentResX);

			Memory::Write(RendererResolutionScanResult + 4, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate renderer resolution scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionHeightInstructionScanResult = Memory::PatternScan(exeModule, "8B 40 10 89 54 24 54 89 44 24 58 A1 E0 A1 13 02");
		if (ViewportResolutionHeightInstructionScanResult)
		{
			spdlog::info("Viewport Resolution Height Instruction: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionHeightInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ViewportResolutionHeightInstructionMidHook{};

			ViewportResolutionHeightInstructionMidHook = safetyhook::create_mid(ViewportResolutionHeightInstructionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution height instruction scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstructionScanResult = Memory::PatternScan(exeModule, "0F BF 50 1C 0F BF 48 1E 89 54 24 4C 8B 50 0C");
		if (ViewportResolutionWidthInstructionScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstructionScanResult + 12 - (std::uint8_t*)exeModule);

			static SafetyHookMid ViewportResolutionWidthInstructionMidHook{};

			ViewportResolutionWidthInstructionMidHook = safetyhook::create_mid(ViewportResolutionWidthInstructionScanResult + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 50 0C 89 54 24 34");
		if (ViewportResolutionWidthInstruction2ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction2ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ViewportResolutionWidthInstruction2MidHook{};

			ViewportResolutionWidthInstruction2MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0xC) = iCurrentResX;
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 2 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionHeightInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 40 10 89 44 24 38");
		if (ViewportResolutionHeightInstruction2ScanResult)
		{
			spdlog::info("Viewport Resolution Height Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionHeightInstruction2ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ViewportResolutionHeightInstruction2MidHook{};

			ViewportResolutionHeightInstruction2MidHook = safetyhook::create_mid(ViewportResolutionHeightInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution height instruction 2 scan memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 4E 68 8B 50 04 D8 76 68 89 56 6C 8B 46 04 85 C0 D9 5E 70");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			static float fLastModifiedHFOV = 0.0f;

			static std::vector<float> vComputedHFOVs;

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

				// Checks if this FOV has already been computed
				if (std::find(vComputedHFOVs.begin(), vComputedHFOVs.end(), fCurrentCameraHFOV) != vComputedHFOVs.end())
				{
					// Value already processed, then skips the calculations
					return;
				}

				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fModifiedHFOVValue = CalculateNewFOV(fCurrentCameraHFOV);

				// If the new computed value is different, updates the FOV value
				if (fCurrentCameraHFOV != fModifiedHFOVValue)
				{
					fCurrentCameraHFOV = fModifiedHFOVValue;
					fLastModifiedHFOV = fModifiedHFOVValue;
				}

				// Stores the new value so future calls can skip re-calculations
				vComputedHFOVs.push_back(fModifiedHFOVValue);

				ctx.ecx = std::bit_cast<uintptr_t>(fCurrentCameraHFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
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
