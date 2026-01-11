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
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "AnimorphsKnowTheSecretWidescreenFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	AKTS,
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
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AKTS, {"Animorphs: Know the Secret", "Animorphs.exe"}},
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

static SafetyHookMid ResolutionWidthInstruction1Hook{};
static SafetyHookMid ResolutionHeightInstruction1Hook{};
static SafetyHookMid ResolutionWidthInstruction8Hook{};
static SafetyHookMid ResolutionHeightInstruction8Hook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::AKTS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 89 44 24 ?? A1 ?? ?? ?? ??", "68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 6A 00 B9 88 55 52 00 E8 14 D6 07", "C7 05 C8 55 52 00 80 02 00 00 C7 05 CC 55 52 00 E0 01 00 00", "68 E0 01 00 00 68 80 02 00 00 6A 00 6A 00 6A 00 B9 88 55 52 00 E8 2A C7 08 00 8B", "68 E0 01 00 00 68 80 02 00 00 50 50 50 B9 88 55 52", "68 E0 01 00 00 68 80 02 00 00 B9 88 55 52 00 E8", "C7 42 40 80 02 00 00 88 42 3D C7 42 44 E0 01 00 00", "8B 6C 24 44 56 85 ED 75 03 8B 69 40 8B 5C 24 4C");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution8Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Resolution1Scan], 6);			

			ResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Resolution1Scan] + 16, 5);			

			ResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan] + 16, [](SafetyHookContext& ctx)
			{					
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 1, iCurrentResY);
			
			Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution4Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution4Scan] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution5Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution5Scan] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution6Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution6Scan] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution7Scan] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution7Scan] + 13, iCurrentResY);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Resolution8Scan], 4);

			ResolutionWidthInstruction8Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution8Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Resolution8Scan] + 12, 4);			

			ResolutionHeightInstruction8Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution8Scan] + 12, [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 74 24 ?? D9 5C 24 ?? D9 44 24 ?? D9 E0");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 4);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FDIV(fNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ??");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x4);

				fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewCameraFOV);
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