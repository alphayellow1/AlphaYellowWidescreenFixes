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
HMODULE dllModule3 = nullptr;
HMODULE dllModule4 = nullptr;
HMODULE dllModule5 = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "BatmanVengeanceWidescreenFix";
std::string sFixVersion = "1.6";
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
float fNewResX;
float fNewGeneralFOV;
float fNewGameplayFOV;
float fNewCameraFOV3;
uintptr_t GameplayFOVAddress1;
uintptr_t GameplayFOVAddress5;

// Game detection
enum class Game
{
	BV,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	GeneralFOV,
	GameplayFOV1,
	GameplayFOV2,
	GameplayFOV3,
	GameplayFOV4,
	GameplayFOV5,
	GameplayFOV6
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BV, {"Batman Vengeance", "Batman.exe"}},
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

	dllModule2 = Memory::GetHandle("osr_dx8_vf.dll", 50);

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid GeneralFOVInstructionHook{};
static SafetyHookMid GameplayFOVInstruction1Hook{};
static SafetyHookMid GameplayFOVInstruction2Hook{};
static SafetyHookMid GameplayFOVInstruction3Hook{};
static SafetyHookMid GameplayFOVInstruction4Hook{};
static SafetyHookMid GameplayFOVInstruction5Hook{};
static SafetyHookMid GameplayFOVInstruction6Hook{};

void GameplayFOVInstructionsMidHook(uintptr_t FOVAddress, uintptr_t& destInstruction)
{
	float& fCurrentGameplayFOV = *reinterpret_cast<float*>(FOVAddress);

	fNewGameplayFOV = fCurrentGameplayFOV * fFOVFactor;

	destInstruction = std::bit_cast<uintptr_t>(fNewGameplayFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::BV && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(dllModule2, "00 00 00 00 02 00 00 80 01 00 00 80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 00 05 00 00 00 04 00 00 40 06 00 00 B0 04 00 00 3C 00 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is osr_dx8_vf.dll+{:x}", ResolutionListScanResult - (std::uint8_t*)dllModule2);

			// 640x480
			Memory::Write(ResolutionListScanResult + 11, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 15, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 19, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 23, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 27, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 31, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		dllModule3 = Memory::GetHandle("enginecore_vf.dll", 50);

		dllModule4 = Memory::GetHandle("aisdk_gamedll_vf.dll", 50);

		dllModule5 = Memory::GetHandle("DLG_vf.dll", 50);

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule5, "8B 46 ?? 8B 4E ?? 8B 96 ?? ?? ?? ?? 53");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is DLG_vf.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule5);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 3);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentResX = *reinterpret_cast<float*>(ctx.esi + 0x24);

				fNewResX = fCurrentResX * fAspectRatioScale;

				ctx.eax = std::bit_cast<uintptr_t>(fNewResX);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule3, "D8 4C 24 ?? D9 5C 24 ?? D9 44 24 ?? D8 74 24", dllModule4, "8B 0D ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 51 52 FF 15 ?? ?? ?? ?? 8B 0D",
		"8B 8E ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 51 52 FF 15 ?? ?? ?? ?? 8B 8E", "68 ?? ?? ?? ?? 52 FF 15 ?? ?? ?? ?? A1", "68 ?? ?? ?? ?? 50 FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 89 8E",
		"8B 0D ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 55");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is enginecore_vr.dll+{:x}", CameraFOVInstructionsScansResult[GeneralFOV] - (std::uint8_t*)dllModule3);

			spdlog::info("Gameplay Camera FOV Instruction 1: Address is aisdk_gamedll_vf.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV1] - (std::uint8_t*)dllModule4);

			spdlog::info("Gameplay Camera FOV Instruction 2: Address is aisdk_gamedll_vf.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV2] - (std::uint8_t*)dllModule4);

			spdlog::info("Gameplay Camera FOV Instruction 3: Address is aisdk_gamedll_vf.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV3] - (std::uint8_t*)dllModule4);

			spdlog::info("Gameplay Camera FOV Instruction 4: Address is aisdk_gamedll_vf.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV4] - (std::uint8_t*)dllModule4);

			spdlog::info("Gameplay Camera FOV Instruction 5: Address is aisdk_gamedll_vf.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV5] - (std::uint8_t*)dllModule4);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GeneralFOV], 4);

			// Instruction is located in the CAM_vAdjustCameraToViewport function
			GeneralFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GeneralFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = *reinterpret_cast<float*>(ctx.esp + 0xC);

				fNewGeneralFOV = fCurrentGeneralFOV * fAspectRatioScale;

				FPU::FMUL(fNewGeneralFOV);
			});

			GameplayFOVAddress1 = (uintptr_t)Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[GameplayFOV1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV1], 6);

			GameplayFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV1], [](SafetyHookContext& ctx)
			{
				GameplayFOVInstructionsMidHook(GameplayFOVAddress1, ctx.ecx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV2], 6);

			GameplayFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV2], [](SafetyHookContext& ctx)
			{
				GameplayFOVInstructionsMidHook(ctx.esi + 0x13C, ctx.ecx);
			});

			fNewCameraFOV3 = 1.299999952f * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[GameplayFOV3] + 1, fNewCameraFOV3);

			Memory::Write(CameraFOVInstructionsScansResult[GameplayFOV4] + 1, fNewCameraFOV3);
			
			GameplayFOVAddress5 = (uintptr_t)Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[GameplayFOV5] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV5], 6);

			GameplayFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV5], [](SafetyHookContext& ctx)
			{
				GameplayFOVInstructionsMidHook(GameplayFOVAddress5, ctx.ecx);
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
