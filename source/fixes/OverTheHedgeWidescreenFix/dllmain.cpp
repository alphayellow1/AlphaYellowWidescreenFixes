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
std::string sFixName = "OverTheHedgeWidescreenFix";
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
float fNewCameraAspectRatio;
float fNewCameraFOV;
float fNewHUDAspectRatio;

// Game detection
enum class Game
{
	OTH,
	Unknown
};

enum HUDAspectRatioInstructionsIndices
{
	HUDAR1Scan,
	HUDAR2Scan,
	HUDAR3Scan,
	HUDAR4Scan,
	HUDAR5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::OTH, {"Over the Hedge", "hedge.exe"}},
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

static SafetyHookMid CameraAspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid CameraFOVInstruction2Hook{};

void WidescreenFix()
{
	if (eGameType == Game::OTH && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		// 1.309999943f , 1.379999995f

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? B0 ?? C2 ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? B0 ?? C2 ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? B0 ?? C2 ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? B0 ?? C2 ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? B0 ?? C2 ?? ?? C7 41");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 640x480
			Memory::Write(ResolutionListScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 10, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 29, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 36, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 55, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 62, iCurrentResY);

			// 1152x864
			Memory::Write(ResolutionListScanResult + 81, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 88, iCurrentResY);

			// 1280x960
			Memory::Write(ResolutionListScanResult + 107, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 114, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 133, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 140, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list scan memory address.");
			return;
		}

		std::uint8_t* CameraAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 8E ?? ?? ?? ?? 52 8B 96");
		if (CameraAspectRatioInstructionScanResult)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraAspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraAspectRatioInstructionHook = safetyhook::create_mid(CameraAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraAspectRatio = *reinterpret_cast<float*>(ctx.esi + 0x2A88);

				fNewCameraAspectRatio = fCurrentCameraAspectRatio * fAspectRatioScale;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraAspectRatio);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 96 ?? ?? ?? ?? 51 52 8B CE");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);			

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x2A84);

				fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> HUDAspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 50 ?? 8B 8E ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 44 24", "68 ?? ?? ?? ?? 50 FF 52 ?? 8B 4E", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 52 ?? 8B 8E ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 5E", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 50 ?? 8B 8E ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 8E", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 52 ?? 8B 8E ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 8E");
		if (Memory::AreAllSignaturesValid(HUDAspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("HUD Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstructionsScansResult[HUDAR1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstructionsScansResult[HUDAR2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstructionsScansResult[HUDAR3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstructionsScansResult[HUDAR4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstructionsScansResult[HUDAR5Scan] - (std::uint8_t*)exeModule);

			fNewHUDAspectRatio = 1.428571463f * fAspectRatioScale;

			Memory::Write(HUDAspectRatioInstructionsScansResult[HUDAR1Scan] + 1, fNewHUDAspectRatio);

			Memory::Write(HUDAspectRatioInstructionsScansResult[HUDAR2Scan] + 1, fNewHUDAspectRatio);

			Memory::Write(HUDAspectRatioInstructionsScansResult[HUDAR3Scan] + 1, fNewHUDAspectRatio);

			Memory::Write(HUDAspectRatioInstructionsScansResult[HUDAR4Scan] + 1, fNewHUDAspectRatio);

			Memory::Write(HUDAspectRatioInstructionsScansResult[HUDAR5Scan] + 1, fNewHUDAspectRatio);
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