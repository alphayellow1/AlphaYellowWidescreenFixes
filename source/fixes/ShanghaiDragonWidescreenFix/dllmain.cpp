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
std::string sFixName = "ShanghaiDragonWidescreenFix";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;

// Game detection
enum class Game
{
	SD,
	Unknown
};

enum ResolutionInstructionsScansIndices
{
	MainMenuResolution1Scan,
	MainMenuResolution2Scan,
	ResolutionListScan
};

enum AspectRatioInstructionsScansIndices
{
	AspectRatio1Scan,
	AspectRatio2Scan
};

enum CameraFOVInstructionsIndices
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SD, {"Shanghai Dragon", "shanghai.exe"}},
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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};

void WidescreenFix()
{
	if (bFixActive == true && eGameType == Game::SD)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8B 11 68", "68 58 02 00 00 8B 4C 24 64 68 20 03 00 00", "C7 47 04 20 03 00 00 C7 47 08 58 02 00 00 EB 1E C7 47 04 00 04 00 00 C7 47 08 00 03 00 00 EB 0E C7 47 04 80 02 00 00 C7 47 08 E0 01 00 00");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Main Menu Resolution 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[MainMenuResolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Main Menu Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[MainMenuResolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResolutionListScan] - (std::uint8_t*)exeModule);

			// Main Menu Resolution
			Memory::Write(ResolutionInstructionsScansResult[MainMenuResolution1Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[MainMenuResolution1Scan] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[MainMenuResolution2Scan] + 10, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[MainMenuResolution2Scan] + 1, iCurrentResY);

			// Resolution List
			// 640x480
			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 35, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 42, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 10, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 19, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResolutionListScan] + 26, iCurrentResY);
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8B 95 ?? ?? ?? ?? 50 8B 85 ?? ?? ?? ?? 51", "D8 89 ?? ?? ?? ?? D9 E8");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio1Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewAspectRatio);
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 85 ?? ?? ?? ?? 51 52 8D 8C 24", "D9 81 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? C7 44 24", "D9 80 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8D 45");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.ebp + 0x33C);

				if (fCurrentCameraFOV1 == 0.8726646304f)
				{
					fNewCameraFOV1 = fCurrentCameraFOV1 * fFOVFactor;
				}
				else
				{
					fNewCameraFOV1 = fCurrentCameraFOV1;
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV1);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x33C);

				if (fCurrentCameraFOV2 == 0.8726646304f)
				{
					fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;
				}
				else
				{
					fNewCameraFOV2 = fCurrentCameraFOV2;
				}

				FPU::FLD(fNewCameraFOV2);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV3Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(ctx.eax + 0x33C);

				if (fCurrentCameraFOV3 == 0.8726646304f)
				{
					fNewCameraFOV3 = fCurrentCameraFOV3 * fFOVFactor;
				}
				else
				{
					fNewCameraFOV3 = fCurrentCameraFOV3;
				}

				FPU::FLD(fNewCameraFOV3);
			});
		}
	}	
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		thisModule = hModule;

		Logging();
		Configuration();		

		if (DetectGame())
		{
			WidescreenFix();
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