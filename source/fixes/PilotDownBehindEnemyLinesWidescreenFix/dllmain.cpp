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
std::string sFixName = "PilotDownBehindEnemyLinesWidescreenFix";
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
constexpr float fNewAspectRatio2 = 0.75f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewCameraFOV2;
float fNewCameraHFOV;

// Game detection
enum class Game
{
	PDBEL_GAME,
	PDBEL_CONFIG,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AR1Scan,
	AR2Scan
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PDBEL_GAME, {"Pilot Down: Behind Enemy Lines", "BEL.exe"}},
	{Game::PDBEL_CONFIG, {"Pilot Down: Behind Enemy Lines - Launcher", "PDLauncher_e.exe"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};

void FOVFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::PDBEL_CONFIG)
		{
			std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? EB ?? C7 44 24 ?? ?? ?? ?? ?? EB ?? C7 44 24 ?? ?? ?? ?? ?? EB");
			if (ResolutionListScanResult)
			{
				spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

				static std::string sNewResString = "-res:" + std::to_string(iCurrentResX) + "," + std::to_string(iCurrentResY);

				const char* cNewResolutionListString = sNewResString.c_str();

				Memory::Write(ResolutionListScanResult + 4, cNewResolutionListString);

				Memory::Write(ResolutionListScanResult + 14, cNewResolutionListString);

				Memory::Write(ResolutionListScanResult + 24, cNewResolutionListString);

				Memory::Write(ResolutionListScanResult + 34, cNewResolutionListString);
			}
			else
			{
				spdlog::error("Failed to locate resolution list scan memory address.");
				return;
			}
		}

		if (eGameType == Game::PDBEL_GAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::vector<std::uint8_t*> AspectRatioInstructionsScanResult = Memory::PatternScan(exeModule, "DB 05 ?? ?? ?? ?? 51 8B C1", "D9 05 ?? ?? ?? ?? D8 74 24 ?? D9 18");
			if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScanResult) == true)
			{
			    spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult[AR1Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult[AR2Scan] - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(AspectRatioInstructionsScanResult[AR1Scan], 6);

				AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScanResult[AR1Scan], [](SafetyHookContext& ctx)
				{
					FPU::FLD(fNewAspectRatio2);
				});

				Memory::WriteNOPs(AspectRatioInstructionsScanResult[AR1Scan] + 9, 6);				

				fNewCameraHFOV = 1.0f / fAspectRatioScale;

				Memory::WriteNOPs(AspectRatioInstructionsScanResult[AR2Scan], 6);

				AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScanResult[AR2Scan], [](SafetyHookContext& ctx)
				{
					FPU::FLD(fNewCameraHFOV);
				});
			}

			std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 81 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC 8B 44 24 ?? 8B 54 24", "D9 44 24 ?? D9 F2");
			if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);
				
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);

				fNewCameraFOV = 3.1f;

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1Scan], 6);

				CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], [](SafetyHookContext& ctx)
				{
					FPU::FLD(fNewCameraFOV);
				});				

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2Scan], 4);

				CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esp + 0x4);

					fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

					FPU::FLD(fNewCameraFOV2);
				});
			}
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