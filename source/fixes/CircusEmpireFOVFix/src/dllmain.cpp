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
HMODULE dllModule2;
HMODULE thisModule;

// Fix details
std::string sFixName = "CircusEmpireFOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float epsilon = 0.00001f;
constexpr float fAspectRatioToCompare = 8.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fNewAspectRatio;

// Game detection
enum class Game
{
	CE,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CE, {"Circus Empire", "Circus.exe"}},
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
	}

	dllModule2 = GetModuleHandleA("LS3DF.dll");
	if (!dllModule2)
	{
		spdlog::error("Failed to get handle for LS3DF.dll.");
		return false;
	}

	spdlog::info("Successfully obtained handle for LS3DF.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid FCOMPInstructionHook{};
void FCOMPInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fcomp dword ptr ds : [fAspectRatioToCompare]
	}
}

static SafetyHookMid FCOMPInstruction2Hook{};
void FCOMPInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fcomp dword ptr ds : [fAspectRatioToCompare]
	}
}

float CalculateNewFOV(float fCurrentFOV)
{
	return fFOVFactor * (2.0f * atanf(tanf(fCurrentFOV / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
}

void FOVFix()
{
	if (eGameType == Game::CE && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(dllModule2, "8B 82 EC 00 00 00 5F 40 5E 89 82 EC 00 00 00 5B C3 D9 82 08 01 00 00");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is LS3DF.dll+{:x}", CameraFOVInstruction1ScanResult + 0x11 - (std::uint8_t*)dllModule2);

			static SafetyHookMid CameraFOVInstruction1MidHook{};

			static float fLastModifiedFOV1 = 0.0f; // Tracks the last modified FOV value

			CameraFOVInstruction1MidHook = safetyhook::create_mid(CameraFOVInstruction1ScanResult + 0x11, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOVValue1 = *reinterpret_cast<float*>(ctx.edx + 0x108);

				if (fCurrentFOVValue1 == 1.22173059f || fCurrentFOVValue1 == 1.256637096f || fCurrentFOVValue1 == 1.086083293f || fCurrentFOVValue1 == 1.139648318f || fCurrentFOVValue1 == 1.130296111f || fCurrentFOVValue1 == 1.087588668f || fCurrentFOVValue1 == 1.256637454f)
				{
					fCurrentFOVValue1 = CalculateNewFOV(fCurrentFOVValue1);
				}

				// Check if the current FOV value was already modified
				if (fCurrentFOVValue1 != fLastModifiedFOV1 && fCurrentFOVValue1 != CalculateNewFOV(1.22173059f) && fCurrentFOVValue1 != CalculateNewFOV(1.256637096f) && fCurrentFOVValue1 != CalculateNewFOV(1.086083293f) && fCurrentFOVValue1 != CalculateNewFOV(1.139648318f) && fCurrentFOVValue1 != CalculateNewFOV(1.130296111f) && fCurrentFOVValue1 != CalculateNewFOV(1.087588668f) && fCurrentFOVValue1 != CalculateNewFOV(1.256637454f))
				{
					// Calculate the new FOV based on aspect ratios
					float fModifiedFOVValue1 = CalculateNewFOV(fCurrentFOVValue1);

					// Update the value only if the modification is meaningful
					if (fCurrentFOVValue1 != fModifiedFOVValue1)
					{
						fCurrentFOVValue1 = fModifiedFOVValue1;
						fLastModifiedFOV1 = fModifiedFOVValue1;
					}
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule2, "D9 82 14 01 00 00 D8 0D ?? ?? ?? ?? D9 82 20 01 00 00");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is LS3DF.dll+{:x}", CameraFOVInstruction2ScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid CameraFOVInstruction2MidHook{};

			static float fLastModifiedFOV2 = 0.0f; // Tracks the last modified FOV value

			CameraFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentFOVValue2 = *reinterpret_cast<float*>(ctx.edx + 0x114);

				// Check if the current FOV value was already modified
				if (fCurrentFOVValue2 != fLastModifiedFOV2)
				{
					// Calculate the new FOV based on aspect ratios
					float fModifiedFOVValue2 = CalculateNewFOV(fCurrentFOVValue2);

					// Update the value only if the modification is meaningful
					if (fCurrentFOVValue2 != fModifiedFOVValue2)
					{
						fCurrentFOVValue2 = fModifiedFOVValue2;
						fLastModifiedFOV2 = fModifiedFOVValue2;
					}
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioComparisonInstruction1ScanResult = Memory::PatternScan(dllModule2, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 41 75 19 D9 C0 DE C1");
		if (AspectRatioComparisonInstruction1ScanResult)
		{
			spdlog::info("Aspect Ratio Comparison Instruction 1: Address is LS3DF.dll+{:x}", AspectRatioComparisonInstruction1ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioComparisonInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			FCOMPInstructionHook = safetyhook::create_mid(AspectRatioComparisonInstruction1ScanResult + 0x6, FCOMPInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio comparison instruction 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioComparisonInstruction2ScanResult = Memory::PatternScan(dllModule2, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 41 75 19 D9 C0 DE C1 D8 15 ?? ?? ?? ?? DF E0 F6 C4 41 75 08 DD D8 D9 05 ?? ?? ?? ?? D9 C0 5F D9 FF D9 C9 D9 FE D9 C9 D9 C9 DE F9");
		if (AspectRatioComparisonInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Comparison Instruction 2: Address is LS3DF.dll+{:x}", AspectRatioComparisonInstruction2ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioComparisonInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			FCOMPInstruction2Hook = safetyhook::create_mid(AspectRatioComparisonInstruction2ScanResult + 0x6, FCOMPInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio comparison instruction 2 memory address.");
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