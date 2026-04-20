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
HMODULE dllModule2 = nullptr;
std::string dllModule2Name;
HMODULE thisModule;

// Fix details
std::string sFixName = "BarbieFashionShowAnEyeForStyleWidescreenFix";
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

// Variables
float fNewCameraFOV;
float fNewAspectRatio;

// Game detection
enum class Game
{
	BFS,
	Unknown
};

enum ResolutionInstructionsScansIndices
{
	Res1,
	Res2,
	Res3,
	Res4
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BFS, {"Barbie Fashion Show: An Eye for Style", "BarbieFashionShow.exe"}}
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

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::BFS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "c7 00 ?? ?? ?? ?? 8a 41",
		"c7 46 ?? ?? ?? ?? ?? c7 46 ?? ?? ?? ?? ?? ff 12 38 5e ?? 74 ?? 8b 46 ?? eb ?? 8b 46 ?? 39 28 75 ?? 38 5e ?? 74 ?? 8b 46 ?? eb ?? 8b 46 ?? 39 78 ?? 75 ?? 8b ce e8 ?? ?? ?? ?? 8b ce e8 ?? ?? ?? ?? eb ?? 8b ce e8 ?? ?? ?? ?? 8b ce e8 ?? ?? ?? ?? 5f 3b c3 5d 88 9e ?? ?? ?? ?? 7d ?? 8b 06",
		"c7 81 ?? ?? ?? ?? ?? ?? ?? ?? c7 81 ?? ?? ?? ?? ?? ?? ?? ?? c3",
		"3d ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? b8 ?? ?? ?? ?? c3 3d ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? b8 ?? ?? ?? ?? c3 3d ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? b8 ?? ?? ?? ?? c3 3d ?? ?? ?? ?? 75 ?? 81 79 ?? ?? ?? ?? ?? 75 ?? b8 ?? ?? ?? ?? c3 3d");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[Res1] + 2, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1] + 12, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res2] + 3, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res2] + 10, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res3] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res3] + 16, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 1, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 10, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 23, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 32, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 45, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 54, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 67, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 76, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 89, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res4] + 98, iCurrentResY);
		}		

		dllModule2 = Memory::GetHandle("PremierRenderDll.dll");

		dllModule2Name = Memory::GetModuleName(dllModule2);

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 56");

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 56");

		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", dllModule2Name.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioInstructionScanResult + 1, "\x05");

			Memory::Write(AspectRatioInstructionScanResult + 2, &fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}	
		
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", dllModule2Name.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);
			
			Memory::WriteNOPs(CameraFOVInstructionScanResult, 6);
			
			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.ecx + 0x104);

				fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

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