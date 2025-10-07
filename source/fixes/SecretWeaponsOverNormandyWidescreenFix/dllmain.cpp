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

// Fix details
std::string sFixName = "SecretWeaponsOverNormandyWidescreenFix";
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
float fNewAspectRatio;
float fAspectRatioScale;
float fCurrentCameraHFOV;
float fCurrentCameraVFOV;
float fNewCameraHFOV;
float fNewCameraVFOV;

// Game detection
enum class Game
{
	SWON,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Resolution1Scan,
	Resolution2Scan,
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan,
	Resolution6Scan,
	Resolution7Scan,
	Resolution8Scan,
	Resolution9Scan
};

enum AspectRatioInstructionsIndex
{
	CameraAspectRatioScan,
	HUDAspectRatio1Scan,
	HUDAspectRatio2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SWON, {"Secret Weapons over Normandy", "Tesla.exe"}},
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

static SafetyHookMid ResolutionWidthInstruction6Hook{};

void ResolutionWidthInstruction6MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResX]
	}
}

static SafetyHookMid ResolutionHeightInstruction6Hook{};

void ResolutionHeightInstruction6MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResY]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::SWON && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "89 6E ?? C7 05", "89 8E ?? ?? ?? ?? 89 96 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 74", "8B 46 ?? A3 ?? ?? ?? ?? 8B 4E ?? 89 0D ?? ?? ?? ?? DB 05", "8B 0F 89 0D ?? ?? ?? ?? 8B 57 04", "8B 4E ?? 89 54 24 ?? 8B 56 ?? 89 54 24 ?? 89 44 24", "DB 41 0C 56 8B 71 10 89 74 24 08 D9 1D ?? ?? ?? ?? 5E DB 44 24 04", "8B 4B 0C A1 ?? ?? ?? ?? 89 4C 24 2C 8B 53 10", "8B 5E ?? 81 FB 80 02 00 00", "8B 6C 24 ?? 56 8b F1 3B 6E");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution9Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan], "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x30) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions2MidHook{};

			ResolutionInstructions2MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x788) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0x78C) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan], "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstruction3MidHook{};

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution3Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan] + 8, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction3MidHook{};

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution3Scan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan], "\x90\x90", 2);

			static SafetyHookMid ResolutionWidthInstruction4MidHook{};

			ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan] + 8, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction4MidHook{};

			ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan], "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstruction5MidHook{};

			ResolutionWidthInstruction5MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan] + 7, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction5MidHook{};

			ResolutionHeightInstruction5MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution6Scan], "\x90\x90\x90", 3);

			ResolutionWidthInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution6Scan], ResolutionWidthInstruction6MidHook);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution6Scan] + 18, "\x90\x90\x90\x90", 4);

			ResolutionHeightInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution6Scan] + 18, ResolutionHeightInstruction6MidHook);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution7Scan], "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstruction7MidHook{};

			ResolutionWidthInstruction7MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution7Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution7Scan] + 12, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction7MidHook{};

			ResolutionHeightInstruction7MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution7Scan] + 12, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution8Scan], "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstruction8MidHook{};

			ResolutionWidthInstruction8MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution8Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution9Scan], "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionWidthInstruction9MidHook{};

			ResolutionWidthInstruction9MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution9Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 08 56 8B 74 24 08 89 4E 68 D8 76 68 8B 50 04");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90", 2);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax);

				fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x4);

				if (fCurrentCameraHFOV == 0.6999999881f && fCurrentCameraVFOV == 0.5249999762f)
				{
					fNewCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale);
				}
				else
				{
					fNewCameraHFOV = fCurrentCameraHFOV * fFOVFactor;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionScanResult + 13, "\x90\x90\x90", 3);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 13, [](SafetyHookContext& ctx)
			{
				fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax);

				fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x4);

				if (fCurrentCameraHFOV == 0.6999999881f && fCurrentCameraVFOV == 0.5249999762f)
				{
					fNewCameraVFOV = fCurrentCameraVFOV;
				}
				else
				{
					fNewCameraVFOV = fCurrentCameraVFOV * fFOVFactor;
				}			

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraVFOV);
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
