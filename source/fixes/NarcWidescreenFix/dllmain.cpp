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
std::string sFixName = "NarcWidescreenFix";
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
constexpr float fOriginalHipfireFOV = 56.0f;

// Ini variables
bool bFixActive;
float fCameraFOVFactor;
float fZoomFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHipfireFOV;
float fNewZoomFOV;
uintptr_t HFOVCullingAddress;
float fNewHFOVCulling;

// Game detection
enum class Game
{
	NARC,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResListUnlock,
	Res_Width_Height
};

enum CameraFOVInstructionsIndices
{
	Hipfire,
	Zoom,
	HFOVCulling
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::NARC, {"Narc", "charlie.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", fCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "ZoomFactor", fZoomFactor);
	spdlog_confparse(fCameraFOVFactor);
	spdlog_confparse(fZoomFactor);

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

static SafetyHookMid ResolutionInstructionsHook{};
static SafetyHookMid HipfireFOVInstructionHook{};
static SafetyHookMid ZoomFOVInstructionHook{};
static SafetyHookMid HFOVCullingInstructionHook{};

void SetARAndFOV()
{
	std::uint8_t* AspectRatioInstructionsScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 5D ?? EB ?? D9 05 ?? ?? ?? ?? D9 5D ?? 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 45 ?? D9 1C 24 A1");
	if (AspectRatioInstructionsScanResult)
	{
		spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult - (std::uint8_t*)exeModule);

		spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult + 11 - (std::uint8_t*)exeModule);

		Memory::Write(AspectRatioInstructionsScanResult + 2, &fNewAspectRatio);

		Memory::Write(AspectRatioInstructionsScanResult + 13, &fNewAspectRatio);
	}
	else
	{
		spdlog::error("Failed to locate aspect ratio instructions scan memory address.");
		return;
	}

	std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 1C 24 8B 8D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 85",
		"D9 44 10 ?? D9 5D ?? D9 45 ?? 8B E5 5D C3 CC CC CC CC 55 8B EC 83 EC ?? 89 4D ?? 8B 45 ?? 83 B8", "D9 05 ?? ?? ?? ?? D9 1C 24 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C3 CC CC CC CC CC CC CC CC 55");
	if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult))
	{
		spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire] - (std::uint8_t*)exeModule);

		spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Zoom] - (std::uint8_t*)exeModule);

		spdlog::info("Camera HFOV Culling Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOVCulling] - (std::uint8_t*)exeModule);

		fNewHipfireFOV = fOriginalHipfireFOV * fCameraFOVFactor;

		Memory::Write(CameraFOVInstructionsScansResult[Hipfire] + 2, &fNewHipfireFOV);

		Memory::WriteNOPs(CameraFOVInstructionsScansResult[Zoom], 4);

		ZoomFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Zoom], [](SafetyHookContext& ctx)
		{
			float& fCurrentZoomFOV = Memory::ReadMem(ctx.eax + ctx.edx + 0x10);

			fNewZoomFOV = fCurrentZoomFOV * fZoomFactor;

			FPU::FLD(fNewZoomFOV);
		});

		// This last hook is still experimental, as I want to fix the culling in a more permanent way
		HFOVCullingAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[HFOVCulling] + 2, Memory::PointerMode::Absolute);

		Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOVCulling], 6);

		HFOVCullingInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOVCulling], [](SafetyHookContext& ctx)
		{
			float& fCurrentHFOVCulling = Memory::ReadMem(HFOVCullingAddress);

			fNewHFOVCulling = fCurrentHFOVCulling / fAspectRatioScale;

			FPU::FLD(fNewHFOVCulling);
		});
	}
}

void FOVFix()
{
	if (eGameType == Game::NARC && bFixActive == true)
	{
		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "81 7D ?? ?? ?? ?? ?? 77 ?? 81 7D", "89 0D ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 85");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResListUnlock] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res_Width_Height] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[ResListUnlock] + 3, 15000);

			Memory::Write(ResolutionInstructionsScansResult[ResListUnlock] + 12, 15000);

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res_Width_Height], [](SafetyHookContext& ctx)
			{
				const int& iCurrentResX = Memory::ReadRegister(ctx.edx);

				const int& iCurrentResY = Memory::ReadRegister(ctx.ecx);

				fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				SetARAndFOV();

				ResolutionInstructionsHook.reset();
			});
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
