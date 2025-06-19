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
HMODULE dllModule2 = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "WingsOfWarFOVFix";
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
constexpr float fAspectRatioToCompare = 8.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewCameraFOV2;

// Game detection
enum class Game
{
	WOW,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WOW, {"Wings of War", "wow.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("LS3DF.dll")) == nullptr)
	{
		spdlog::warn("LS3DF.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for LS3DF.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

float CalculateNewFOV(float fCurrentFOV)
{
	return fFOVFactor * (2.0f * atanf(tanf(fCurrentFOV / 2.0f) * fAspectRatioScale));
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.edx + 0x108);

	fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.edx + 0x114);

	fNewCameraFOV2 = CalculateNewFOV(fCurrentCameraFOV2);

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid FCOMPInstructionHook{};

void FCOMPInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fcomp dword ptr ds:[fAspectRatioToCompare]
	}
}

static SafetyHookMid FCOMPInstruction2Hook{};

void FCOMPInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fcomp dword ptr ds:[fAspectRatioToCompare]
	}
}

void FOVFix()
{
	if (eGameType == Game::WOW && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 82 EC 00 00 00 5F 40 5E 89 82 EC 00 00 00 5B C3 D9 82 08 01 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is LS3DF.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult + 17, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 23, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule2, "8D B2 2C 03 00 00 33 C0 B9 10 00 00 00 8B FE F3 AB D9 82 14 01 00 00");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is LS3DF.dll+{:x}", CameraFOVInstruction2ScanResult + 17 - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult + 17, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 23, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioComparisonInstructionScanResult = Memory::PatternScan(dllModule2, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 41 75 19 D9 C0 DE C1 D8 15 ?? ?? ?? ?? DF E0 F6 C4 41 75 08 DD D8 D9 05 ?? ?? ?? ?? D9 C0 8D BA AC 02 00 00 D9 FF B9 10 00 00 00 8B F3 D9 C9 D9 FE D9 C9 D9 C9 DE F9 D9 C0 D8 8A 20 01 00 00 D9 C1");
		if (AspectRatioComparisonInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Comparison Instruction: Address is LS3DF.dll+{:x}", AspectRatioComparisonInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioComparisonInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			FCOMPInstructionHook = safetyhook::create_mid(AspectRatioComparisonInstructionScanResult + 6, FCOMPInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio comparison instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatioComparisonInstruction2ScanResult = Memory::PatternScan(dllModule2, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 41 75 19 D9 C0 DE C1 D8 15 ?? ?? ?? ?? DF E0 F6 C4 41 75 08 DD D8 D9 05 ?? ?? ?? ?? D9 C0 5F D9 FF D9 C9 D9 FE D9 C9 D9 C9 DE F9 D9 C0 D8 8A 20 01 00 00 D9 C1 D9 1E 5E 5B D9 92 40 03 00 00");
		if (AspectRatioComparisonInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Comparison Instruction 2: Address is LS3DF.dll+{:x}", AspectRatioComparisonInstruction2ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioComparisonInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			FCOMPInstruction2Hook = safetyhook::create_mid(AspectRatioComparisonInstruction2ScanResult + 6, FCOMPInstruction2MidHook);
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