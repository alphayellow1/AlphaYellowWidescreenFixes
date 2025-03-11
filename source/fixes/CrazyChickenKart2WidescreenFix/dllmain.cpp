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
#include <vector>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "CrazyChickenKart2WidescreenFix";
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
constexpr float epsilon = 0.00001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fModifiedHFOVValue;
float fModifiedVFOVValue;

// Game detection
enum class Game
{
	CCK2,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CCK2, {"Crazy Chicken: Kart 2", "MHK2-XXL.exe"}},
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
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
	return false;
}

void WidescreenFix()
{
	if (eGameType == Game::CCK2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionWidthInstruction1ScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 8B 57 04");
		if (ResolutionWidthInstruction1ScanResult)
		{
			spdlog::info("Resolution Width Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstruction1ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionWidthInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uint32_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution width instruction 1 memory address.");
			return;
		}

		std::uint8_t* ResolutionWidthInstruction2ScanResult = Memory::PatternScan(exeModule, "DB 40 0C C3 CC CC CC CC CC CC CC");
		if (ResolutionWidthInstruction2ScanResult)
		{
			spdlog::info("Resolution Width Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionWidthInstruction2ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionWidthInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iNewResolutionWidth = *reinterpret_cast<uint32_t*>(ctx.eax + 0xC);

				iNewResolutionWidth = iCurrentResX;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution width instruction 2 memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstruction1ScanResult = Memory::PatternScan(exeModule, "89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? 02 00 00 00");
		if (ResolutionHeightInstruction1ScanResult)
		{
			spdlog::info("Resolution Height Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstruction1ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionHeightInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uint32_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution height instruction 1 memory address.");
			return;
		}

		std::uint8_t* ResolutionHeightInstruction2ScanResult = Memory::PatternScan(exeModule, "DB 40 10 C3 CC CC CC CC CC CC CC");
		if (ResolutionHeightInstruction2ScanResult)
		{
			spdlog::info("Resolution Height Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionHeightInstruction2ScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionHeightInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iNewResolutionHeight = *reinterpret_cast<uint32_t*>(ctx.eax + 0x10);

				iNewResolutionHeight = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution height instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 76 68 89 56 6C 8B 46 04 85 C0 D9 5E 70");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			static float fLastModifiedHFOV = 0.0f;

			static std::vector<float> vComputedHFOVs;

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.esi + 0x68);

				// Checks if this FOV has already been computed
				if (std::find(vComputedHFOVs.begin(), vComputedHFOVs.end(), fCurrentCameraHFOV) != vComputedHFOVs.end())
				{
					// Value already processed, then skips the calculations
					return;
				}

				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fModifiedHFOVValue = fCurrentCameraHFOV / (fOldAspectRatio / fNewAspectRatio);

				// If the new computed value is different, updates the FOV value
				if (fCurrentCameraHFOV != fModifiedHFOVValue)
				{
					fCurrentCameraHFOV = fModifiedHFOVValue;
					fLastModifiedHFOV = fModifiedHFOVValue;
				}

				// Stores the new value so future calls can skip re-calculations
				vComputedHFOVs.push_back(fModifiedHFOVValue);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 76 6C D9 5E 74 74 09 50");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			static float fLastModifiedVFOV = 0.0f;

			static std::vector<float> vComputedVFOVs;

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.esi + 0x6C);

				// Checks if this FOV has already been computed
				if (std::find(vComputedVFOVs.begin(), vComputedVFOVs.end(), fCurrentCameraVFOV) != vComputedVFOVs.end())
				{
					// Value already processed, then skips the calculations
					return;
				}

				// Computes the new FOV value if the current FOV is different from the last modified FOV
				fModifiedVFOVValue = fCurrentCameraVFOV / (fOldAspectRatio / fNewAspectRatio);

				// If the new computed value is different, updates the FOV value
				if (fCurrentCameraVFOV != fModifiedVFOVValue)
				{
					fCurrentCameraVFOV = fModifiedVFOVValue;
					fLastModifiedVFOV = fModifiedVFOVValue;
				}

				// Stores the new value so future calls can skip re-calculations
				vComputedVFOVs.push_back(fModifiedVFOVValue);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction memory address.");
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