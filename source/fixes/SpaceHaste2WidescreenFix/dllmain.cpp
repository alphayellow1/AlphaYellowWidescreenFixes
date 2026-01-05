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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "SpaceHaste2WidescreenFix";
std::string sFixVersion = "1.2";
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
	SH2,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Resolution512x384Scan1,
	Resolution512x384Scan2,
	Resolution512x384Scan3,
	Resolution512x384Scan4,
	Resolution640x480Scan,
	Resolution800x600Scan,
	Resolution1024x768Scan,
	Resolution1280x1024Scan,
	Resolution1600x1200Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SH2, {"Space Haste 2", "SpaceHaste.exe"}},
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
	bool bGameFound = false;

	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			bGameFound = true;
			break;
		}
	}

	if (bGameFound == false)
	{
		spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		return false;
	}

	dllModule2 = Memory::GetHandle("trend.dll");

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::SH2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "50 68 80 01 00 00 68 00 02 00 00 51 8B 0D ?? ?? ?? ??", "CF 51 68 80 01 00 00 68 00 02 00 00 52 E9 9A", "A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 68 80 01 00 00 68 00 02 00 00", "47 00 51 68 80 01 00 00 68 00 02 00 00 52 EB 76", 
		"50 68 E0 01 00 00 68 80 02 00 00 51 8B 0D ?? ?? ?? ??", "50 68 58 02 00 00 68 20 03 00 00 51 8B 0D ?? ?? ?? ??", "50 68 00 03 00 00 68 00 04 00 00 51 8B 0D ?? ?? ?? ??", "50 68 00 04 00 00 68 00 05 00 00 51 8B 0D ?? ?? ?? ??", "50 68 B0 04 00 00 68 40 06 00 00 51 8B 0D ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution 512x384 Scan 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution512x384Scan1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 512x384 Scan 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution512x384Scan2] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 512x384 Scan 3: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution512x384Scan3] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 512x384 Scan 4: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution512x384Scan4] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 640x480 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution640x480Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 800x600 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution800x600Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 1024x768 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1024x768Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 1280x1024 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1280x1024Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 1600x1200 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1600x1200Scan] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan1] + 2, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan1] + 7, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan2] + 3, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan2] + 8, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan3] + 13, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan3] + 18, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan4] + 4, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution512x384Scan4] + 9, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution640x480Scan] + 2, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution640x480Scan] + 7, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution800x600Scan] + 2, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution800x600Scan] + 7, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1024x768Scan] + 2, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1024x768Scan] + 7, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1280x1024Scan] + 2, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1280x1024Scan] + 7, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1600x1200Scan] + 2, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1600x1200Scan] + 7, iCurrentResX);
		}

		// Both aspect ratio and FOV instructions are located in twnd::SetAspect function
		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "8B 50 04 D8 3D ?? ?? ?? ?? 89 15 ?? ?? ?? ??");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is trend.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90", 3);			

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 40 08 A3 ?? ?? ?? ?? 57 33 C0 B9 10 00 00 00 BF ?? ?? ?? ??");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is trend.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);			

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0x8);

				if ((fCurrentCameraFOV >= 0.5925768017768860f && fCurrentCameraFOV <= 0.7399606704711914f) && (fCurrentCameraFOV != 0.7002075314521790f && fCurrentCameraFOV != 0.6999999880790710f))
				{
					fNewCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraFOV, fAspectRatioScale);
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
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