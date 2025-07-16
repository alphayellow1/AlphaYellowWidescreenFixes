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
std::string sFixName = "ReservoirDogsFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalHipfireCameraFOV = 70.0f;
constexpr float fOriginalZoomCameraFOV = 50.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fZoomFactor;
float fFramerate;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fAspectRatioScale;
float fNewHipfireCameraFOV;
float fNewZoomCameraFOV;
float fNewHipfireCameraFOV2;
static uint8_t* HipfireCameraFOVValueAddress;
static uint8_t* ZoomCameraFOVValueAddress;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
	return degrees * (fPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
	return radians * (180.0f / fPi);
}

// Game detection
enum class Game
{
	RD,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RD, {"Reservoir Dogs", "ResDogs.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "ZoomFactor", fZoomFactor);
	inipp::get_value(ini.sections["Settings"], "Framerate", fFramerate);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fZoomFactor);
	spdlog_confparse(fFramerate);

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

float CalculateNewHFOVWithoutFOVFactor(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * fAspectRatioScale));
}

float CalculateNewHFOVWithFOVFactor(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf((fFOVFactor * tanf(DegToRad(fCurrentFOV / 2.0f))) * fAspectRatioScale));
}

float CalculateNewVFOVWithoutFOVFactor(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f))));
}

float CalculateNewVFOVWithFOVFactor(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * fFOVFactor));
}

static SafetyHookMid CameraHFOVInstructionHook{};

void CameraHFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.esi + 0x34);

	fNewCameraHFOV = CalculateNewHFOVWithoutFOVFactor(fCurrentCameraHFOV);

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV]
	}
}

static SafetyHookMid CameraVFOVInstructionHook{};

void CameraVFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.esi + 0x38);

	fNewCameraVFOV = CalculateNewVFOVWithoutFOVFactor(fCurrentCameraVFOV);

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV]
	}
}

static SafetyHookMid HipfireCameraFOVInstruction2Hook{};

void HipfireCameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentHipfireCameraFOV = *reinterpret_cast<float*>(HipfireCameraFOVValueAddress);

	fNewHipfireCameraFOV2 = fCurrentHipfireCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewHipfireCameraFOV2]
	}
}

static SafetyHookMid ZoomCameraFOVInstructionHook{};

void ZoomCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentZoomCameraFOV = *reinterpret_cast<float*>(ZoomCameraFOVValueAddress);

	fNewZoomCameraFOV = fCurrentZoomCameraFOV * (1.0f / fZoomFactor);

	_asm
	{
		fld dword ptr ds:[fNewZoomCameraFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::RD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 46 34 D8 0D ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? 83 C4 08");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90", 3);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, CameraHFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 46 38 83 EC 08 D8 0D ?? ?? ?? ?? D9 5C 24 04");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionScanResult, "\x90\x90\x90", 3);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, CameraVFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera VFOV instruction memory address.");
			return;
		}

		std::uint8_t* HipfireCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "68 00 00 8C 42 E8 ?? ?? ?? ?? 89 BE 08 13 00 00 F3 0F 10 05 08");
		if (HipfireCameraFOVInstructionScanResult)
		{
			spdlog::info("Hipfire Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), HipfireCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			fNewHipfireCameraFOV = fOriginalHipfireCameraFOV * fFOVFactor;

			Memory::Write(HipfireCameraFOVInstructionScanResult + 1, fNewHipfireCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate hipfire camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* HipfireCameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? F3 0F 10 15 ?? ?? ?? ?? F3 0F 5C D1 F3 0F 59 D0 F3 0F 58 D1");
		if (HipfireCameraFOVInstruction2ScanResult)
		{
			spdlog::info("Hipfire Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), HipfireCameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			uint32_t imm1 = *reinterpret_cast<uint32_t*>(HipfireCameraFOVInstruction2ScanResult + 2);

			HipfireCameraFOVValueAddress = reinterpret_cast<uint8_t*>(imm1);

			Memory::PatchBytes(HipfireCameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			HipfireCameraFOVInstruction2Hook = safetyhook::create_mid(HipfireCameraFOVInstruction2ScanResult, HipfireCameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate hipfire camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* ZoomCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? F3 0F 10 1D ?? ?? ?? ?? F3 0F 5C DA F3 0F 59 D9 F3 0F 58 DA");
		if (ZoomCameraFOVInstructionScanResult)
		{
			spdlog::info("Zoom Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), ZoomCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			uint32_t imm2 = *reinterpret_cast<uint32_t*>(ZoomCameraFOVInstructionScanResult + 2);

			ZoomCameraFOVValueAddress = reinterpret_cast<uint8_t*>(imm2);

			Memory::PatchBytes(ZoomCameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			ZoomCameraFOVInstructionHook = safetyhook::create_mid(ZoomCameraFOVInstructionScanResult, ZoomCameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate zoom camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* FramerateInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 10 05 ?? ?? ?? ?? 89 86 2C 41 00 00 89 BE 08 41 00 00 89 BE 0C 41 00 00 89 BE 10 41 00 00 89 BE 14 41 00 00 89 BE 20 41 00 00 A1 ?? ?? ?? ?? F3 0F 11 40 20 8B 0D ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??");
		if (FramerateInstructionScanResult)
		{
			spdlog::info("Framerate Instruction: Address is {:s}+{:x}", sExeName.c_str(), FramerateInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(FramerateInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			static SafetyHookMid FramerateInstruction1MidHook{};

			FramerateInstruction1MidHook = safetyhook::create_mid(FramerateInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = fFramerate / 2.0f;
			});

			Memory::PatchBytes(FramerateInstructionScanResult + 60, "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			static SafetyHookMid FramerateInstruction2MidHook{};

			FramerateInstruction2MidHook = safetyhook::create_mid(FramerateInstructionScanResult + 60, [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = fFramerate;
			});
		}
		else
		{
			spdlog::error("Failed to locate framerate instruction memory address.");
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