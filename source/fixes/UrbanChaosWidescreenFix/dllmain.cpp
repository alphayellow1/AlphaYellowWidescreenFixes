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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "UrbanChaosWidescreenFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fAspectRatioScale;
float fNewAspectRatio;
float fNewAspectRatio2;
float fNewAspectRatio3;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCullingX;
float f640Res;
uint8_t* AspectRatioAddress;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	UC,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::UC, {"Urban Chaos", "Fallen.exe"}},
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

static SafetyHookMid AspectRatioInstruction2Hook{};

void AspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fdiv dword ptr ds:[f640Res]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::UC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;	

		std::uint8_t* ResolutionStringScanResult = Memory::PatternScan(exeModule, "31 30 32 34 20 78 20 37 36 38 00");
		if (ResolutionStringScanResult)
		{
			spdlog::info("Resolution String Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionStringScanResult - (std::uint8_t*)exeModule);

			if ((Maths::digitCount(iCurrentResX) == 4 && Maths::digitCount(iCurrentResY) == 4) || (Maths::digitCount(iCurrentResX) == 4 && Maths::digitCount(iCurrentResY) == 3))
			{
				Maths::WriteNumberAsChar8Digits(ResolutionStringScanResult, iCurrentResX);

				Maths::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, iCurrentResY);
			}

			if (Maths::digitCount(iCurrentResX) == 3 && Maths::digitCount(iCurrentResY) == 3)
			{
				Memory::PatchBytes(ResolutionStringScanResult, "\x00", 1);

				Maths::WriteNumberAsChar8Digits(ResolutionStringScanResult + 1, iCurrentResX);

				Maths::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, iCurrentResY);
			}
		}
		else
		{
			spdlog::error("Failed to locate resolution string scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "BF 00 04 00 00 BE 00 03 00 00");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScanResult + 1, iCurrentResX);

			Memory::Write(ResolutionInstructionsScanResult + 6, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}
		
		std::uint8_t* AspectRatioAndCameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "8B 15 ?? ?? ?? ?? A1 ?? ?? ?? ?? 52 50 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? A1 ?? ?? ?? ??");
		if (AspectRatioAndCameraFOVInstructionsScanResult)
		{
			spdlog::info("Aspect Ratio & Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioAndCameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			AspectRatioAddress = Memory::GetPointer<uint32_t>(AspectRatioAndCameraFOVInstructionsScanResult + 7, Memory::PointerMode::Absolute);

			CameraFOVAddress = Memory::GetPointer<uint32_t>(AspectRatioAndCameraFOVInstructionsScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(AspectRatioAndCameraFOVInstructionsScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid AspectRatioAndCameraFOVInstructions1Hook{};

			AspectRatioAndCameraFOVInstructions1Hook = safetyhook::create_mid(AspectRatioAndCameraFOVInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio1 = *reinterpret_cast<float*>(AspectRatioAddress);

				fNewAspectRatio2 = fCurrentAspectRatio1 / fAspectRatioScale;

				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio2);

				float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(CameraFOVAddress);

				if (fCurrentCameraFOV1 == 1.875f)
				{
					fNewCameraFOV1 = fCurrentCameraFOV1 / fFOVFactor;
				}
				else
				{
					fNewCameraFOV1 = fCurrentCameraFOV1;
				}

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV1);
			});

			Memory::PatchBytes(AspectRatioAndCameraFOVInstructionsScanResult + 24, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid AspectRatioAndCameraFOVInstructions2Hook{};

			AspectRatioAndCameraFOVInstructions2Hook = safetyhook::create_mid(AspectRatioAndCameraFOVInstructionsScanResult + 24, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio2 = *reinterpret_cast<float*>(AspectRatioAddress);

				fNewAspectRatio3 = fCurrentAspectRatio2 / fAspectRatioScale;

				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio3);

				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(CameraFOVAddress);

				if (fCurrentCameraFOV2 == 1.875f)
				{
					fNewCameraFOV2 = fCurrentCameraFOV2 / fFOVFactor;
				}
				else
				{
					fNewCameraFOV2 = fCurrentCameraFOV2;
				}

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV2);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio & camera FOV instructions scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 1D 88 28 EB 00 D9 05 30 19 E8 00 D8 35 98 19 E8 00");
		if (AspectRatioInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction2ScanResult - (std::uint8_t*)exeModule);

			f640Res = 640.0f;

			Memory::PatchBytes(AspectRatioInstruction2ScanResult + 12, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstruction2ScanResult + 12, AspectRatioInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 2 memory address.");
			return;
		}

		std::uint8_t* CullingCameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "C7 05 ?? ?? ?? ?? 00 00 20 44 A3 ?? ?? ?? ?? 83 E8 00 C7 05 ?? ?? ?? ?? 00 00 00 00 C7 05 ?? ?? ?? ?? 00 00 20 44");
		if (CullingCameraFOVInstructionsScanResult)
		{
			spdlog::info("Culling Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CullingCameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			fNewCullingX = (float)iCurrentResX;

			Memory::Write(CullingCameraFOVInstructionsScanResult + 6, fNewCullingX);
		}
		else
		{
			spdlog::error("Failed to locate culling camera FOV instructions scan memory address.");
			return;
		}

		std::uint8_t* LightingCacheCrashFixScanResult = Memory::PatternScan(exeModule, "00 80 CC 40 89 45 00");
		if (LightingCacheCrashFixScanResult)
		{
			spdlog::info("Lighting Cache Crash Fix Scan: Address is {:s}+{:x}", sExeName.c_str(), LightingCacheCrashFixScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(LightingCacheCrashFixScanResult + 3, "\x00", 1);
		}
		else
		{
			spdlog::error("Failed to locate lighting cache crash fix scan memory address.");
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