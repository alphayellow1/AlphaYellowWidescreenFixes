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
std::string sFixName = "StateOfEmergencyWidescreenFix";
std::string sFixVersion = "1.5.1";
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
float fNewHorizontalRenderingValue;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewHorizontalCulling;
float fNewFOVCulling1;
float fNewFOVCulling2;
uint8_t* ResolutionWidthAddress;
uint8_t* ResolutionHeightAddress;
uint8_t* HorizontalCullingAddress;

// Game detection
enum class Game
{
	SOE,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SOE, {"State of Emergency", "KaosPC.exe"}},
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

static SafetyHookMid CullingInstructions1Hook{};

void CullingInstructions1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentHorizontalCulling = *reinterpret_cast<float*>(HorizontalCullingAddress);

	fNewHorizontalCulling = (fCurrentHorizontalCulling / fAspectRatioScale) / fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewHorizontalCulling]
	}

	float& fCurrentFOVCulling1 = *reinterpret_cast<float*>(ctx.esp + 0x50);

	fNewFOVCulling1 = 50.0f;

	_asm
	{
		fmul dword ptr ds:[fNewFOVCulling1]
	}
}

static SafetyHookMid CullingInstructions2Hook{};

void CullingInstructions2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentFOVCulling2 = *reinterpret_cast<float*>(ctx.esp + 0x50);

	fNewFOVCulling2 = 50.0f;

	_asm
	{
		fld dword ptr ds:[fNewFOVCulling2]
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ebp + 0xC);

	fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::SOE && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "8B 04 10 57 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6B C0 ?? 8B 0E 8B 54 08 ??");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstructionMidHook{};

			ResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScanResult + 19, "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionHeightInstructionMidHook{};

			ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 19, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "C7 44 24 ?? 00 00 A0 3F D9 44 24 ?? 8B 44 24 ??");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionScanResult + 4, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CullingInstructions1ScanResult = Memory::PatternScan(exeModule, "D9 05 AC 49 4C 00 D8 4C 24 50");
		if (CullingInstructions1ScanResult)
		{
			spdlog::info("Culling Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), CullingInstructions1ScanResult - (std::uint8_t*)exeModule);

			HorizontalCullingAddress = Memory::GetPointerFromAddress<uint32_t>(CullingInstructions1ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CullingInstructions1ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10);

			CullingInstructions1Hook = safetyhook::create_mid(CullingInstructions1ScanResult, CullingInstructions1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate culling instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* CullingInstructions2ScanResult = Memory::PatternScan(exeModule, "D9 44 24 50 C7 05 D0 E4 4D 00 00 00 00 00 D9 E0 C7 05 CC E4 4D 00 00 00 00 00");
		if (CullingInstructions2ScanResult)
		{
			spdlog::info("Culling Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), CullingInstructions2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CullingInstructions2ScanResult, "\x90\x90\x90\x90", 4);
			
			CullingInstructions2Hook = safetyhook::create_mid(CullingInstructions2ScanResult, CullingInstructions2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate culling instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 45 0C 8D 45 0C D8 0D ?? ?? ?? ?? 89 45 F4 8D 45 FC");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);	

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
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
