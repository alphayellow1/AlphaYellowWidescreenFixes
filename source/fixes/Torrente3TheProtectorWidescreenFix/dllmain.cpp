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
std::string sFixName = "Torrente3TheProtectorWidescreenFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
float fNewCameraFOV;
uintptr_t CameraFOV1Address;
uintptr_t CameraFOV2Address;
uintptr_t CameraFOV3Address;
uintptr_t CameraFOV4Address;

// Game detection
enum class Game
{
	T3TP,
	Unknown
};

enum ResolutionsInstructionsIndices
{
	ResList1Scan,
	ResList2Scan
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan,
	FOV4Scan,
	FOV5Scan,
	FOV6Scan,
	FOV7Scan,
	FOV8Scan,
	FOV9Scan,
	FOV10Scan,
	FOV11Scan,
	FOV12Scan,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::T3TP, {"Torrente 3: The Protector", "Torrente3.exe"}},
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
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};
static SafetyHookMid CameraFOVInstruction7Hook{};
static SafetyHookMid CameraFOVInstruction8Hook{};
static SafetyHookMid CameraFOVInstruction9Hook{};
static SafetyHookMid CameraFOVInstruction10Hook{};
static SafetyHookMid CameraFOVInstruction11Hook{};
static SafetyHookMid CameraFOVInstruction12Hook{};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float& DestInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

	DestInstruction = fNewCameraFOV;
}

void WidescreenFix()
{
	if (eGameType == Game::T3TP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionListsScansResult = Memory::PatternScan(exeModule, "BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? 8D 54 24", "BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? 39 2D");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResList1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResList2Scan] - (std::uint8_t*)exeModule);

			// Resolution List 1
			// 640x480
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 1, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 6, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 13, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 18, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 25, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 30, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 37, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 42, iCurrentResY);

			// Resolution List 2
			// 640x480
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 1, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 6, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 13, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 18, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 25, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 30, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 37, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 42, iCurrentResY);
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			Memory::Write(AspectRatioInstructionScanResult + 1, fNewAspectRatio2);
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "F3 0F 10 05 ?? ?? ?? ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? 8B 56 ?? 8B 8A ?? ?? ?? ??", "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 8E ?? ?? ?? ?? 8B 49 ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??",
		"F3 0F 10 05 ?? ?? ?? ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? 8B 4E ??", "F3 0F 10 05 ?? ?? ?? ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? 8B 46 ?? 8B 48 ??", "F3 0F 10 05 ?? ?? ?? ?? 89 48 ?? 8B 7F ?? 0F 2E 47 ??", "F3 0F 10 44 24 ?? 83 C4 ?? 0F 2E 41 ?? 9F F6 C4 ??", "F3 0F 10 05 ?? ?? ?? ?? 89 48 ?? 8B 4F ?? EB ?? 84 C0 0F 85 ?? ?? ?? ??",
		"F3 0F 10 05 ?? ?? ?? ?? 8B 56 ?? F3 0F 11 86 ?? ?? ?? ?? 89 4E ?? 8B 82 ?? ?? ?? ?? C1 E8 ?? A8 ??", "F3 0F 10 05 ?? ?? ?? ?? 8D 4C 24 ?? 51 6A ?? 8D 54 24 ?? 52 8D 44 24 ??", "F3 0F 10 05 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 46 ?? 89 46 ?? 89 46 ??", "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? 8B 76 ??", "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 42 ?? 8B 51 ?? 88 41 ??");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV8Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV9Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV10Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 11: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV11Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 12: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV12Scan] - (std::uint8_t*)exeModule);

			CameraFOV1Address = (uintptr_t)Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV1Scan] + 4, Memory::PointerMode::Absolute);

			CameraFOV2Address = (uintptr_t)Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV3Scan] + 4, Memory::PointerMode::Absolute);

			CameraFOV3Address = (uintptr_t)Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV5Scan] + 4, Memory::PointerMode::Absolute);

			CameraFOV4Address = (uintptr_t)Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV7Scan] + 4, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1Scan], 8);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2Scan], 8);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3Scan], 8);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV2Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV4Scan], 8);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV5Scan], 8);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV3Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6Scan], 6);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x44, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV7Scan], 8);

			CameraFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV7Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV4Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV8Scan], 8);

			CameraFOVInstruction8Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV8Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV9Scan], 8);

			CameraFOVInstruction9Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV9Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV10Scan], 8);

			CameraFOVInstruction10Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV10Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV11Scan], 8);

			CameraFOVInstruction11Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV11Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV12Scan], 8);

			CameraFOVInstruction12Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV12Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV1Address, ctx.xmm0.f32[0], ctx);
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