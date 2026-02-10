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
constexpr float fOriginalHipfireCameraFOV = 70.0f;
constexpr float fOriginalZoomCameraFOV = 50.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fZoomFactor;
float fFramerate;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHUDAspectRatio;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fNewHipfireCameraFOV;
float fNewZoomCameraFOV;
float fNewHipfireCameraFOV2;
uint8_t* HipfireFOV2Address;
uint8_t* ZoomFOVAddress;

// Game detection
enum class Game
{
	RD,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	HFOV,
	VFOV,
	HipfireFOV1,
	HipfireFOV2,
	ZoomFOV
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

static SafetyHookMid HUDAspectRatioInstructionHook{};
static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};
static SafetyHookMid HipfireCameraFOVInstruction2Hook{};
static SafetyHookMid ZoomCameraFOVInstructionHook{};
static SafetyHookMid FramerateInstruction1Hook{};
static SafetyHookMid FramerateInstruction2Hook{};

void FOVFix()
{
	if (eGameType == Game::RD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 46 ?? D8 0D ?? ?? ?? ?? D9 1C ?? E8", "D9 46 ?? 83 EC ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 46", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 BE ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??", "D9 05 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? F3 0F 10 15", "D9 05 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? F3 0F 10 1D");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV] - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HipfireFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HipfireFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Zoom Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[ZoomFOV] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV], 3);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.esi + 0x34);

				fNewCameraHFOV = Maths::CalculateNewHFOV_DegBased(fCurrentCameraHFOV, fAspectRatioScale);

				FPU::FLD(fNewCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV], 3);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.esi + 0x38);

				fNewCameraVFOV = Maths::CalculateNewVFOV_DegBased(fCurrentCameraVFOV);

				FPU::FLD(fNewCameraVFOV);
			});

			fNewHipfireCameraFOV = fOriginalHipfireCameraFOV * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[HipfireFOV1] + 1, fNewHipfireCameraFOV);

			HipfireFOV2Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[HipfireFOV2] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HipfireFOV2], 6);

			HipfireCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireFOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireCameraFOV2 = *reinterpret_cast<float*>(HipfireFOV2Address);

				fNewHipfireCameraFOV2 = fCurrentHipfireCameraFOV2 * fFOVFactor;

				FPU::FLD(fNewHipfireCameraFOV2);
			});

			ZoomFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[ZoomFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[ZoomFOV], 6);

			ZoomCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ZoomFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomCameraFOV = *reinterpret_cast<float*>(ZoomFOVAddress);

				fNewZoomCameraFOV = fCurrentZoomCameraFOV / fZoomFactor;

				FPU::FLD(fNewZoomCameraFOV);
			});
		}

		std::uint8_t* FramerateInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 10 05 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 BE");
		if (FramerateInstructionScanResult)
		{
			spdlog::info("Framerate Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), FramerateInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(FramerateInstructionScanResult, 8);

			FramerateInstruction1Hook = safetyhook::create_mid(FramerateInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = fFramerate / 2.0f;
			});

			Memory::WriteNOPs(FramerateInstructionScanResult + 60, 8);			

			FramerateInstruction2Hook = safetyhook::create_mid(FramerateInstructionScanResult + 60, [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = fFramerate;
			});
		}
		else
		{
			spdlog::error("Failed to locate framerate instructions scan memory address.");
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