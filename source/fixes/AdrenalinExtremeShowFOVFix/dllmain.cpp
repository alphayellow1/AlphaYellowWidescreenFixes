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
HMODULE thisModule;

// Fix details
std::string sFixName = "AdrenalinExtremeShowFOVFix";
std::string sFixVersion = "1.3";
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
float fNewCameraFOV1;
float fNewCameraFOV2;

// Game detection
enum class Game
{
	AES,
	Unknown
};

enum AspectRatioInstructionsIndex
{
	AR1Scan,
	AR2Scan,
	AR3Scan,
	AR4Scan
};

enum CameraFOVInstructionsIndex
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
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AES, {"Adrenalin: Extreme Show", "Adrenalin.exe"}},
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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};
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

void CameraFOVInstructions1MidHook(uintptr_t FOVAddress)
{
	float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(FOVAddress);

	fNewCameraFOV1 = fCurrentCameraFOV1 / fFOVFactor;

	FPU::FLD(fNewCameraFOV1);
}

void CameraFOVInstructions2MidHook(uintptr_t SourceAddress, uintptr_t& DestinationAddress)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(SourceAddress);

	fNewCameraFOV2 = fCurrentCameraFOV2 / fFOVFactor;

	DestinationAddress = std::bit_cast<uintptr_t>(fNewCameraFOV2);
}

void FOVFix()
{
	if (eGameType == Game::AES && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 F9 D9 1C ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE F9 D9 5C 24 ?? 89 6C 24", "D9 05 ?? ?? ?? ?? D8 F9 D9 1C ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE F9 D9 5C 24 ?? 89 44 24", 
		"D8 05 ?? ?? ?? ?? DB 44 24 ?? DE C9 D9 5C 24 ?? D9 44 24 ?? DB 5C 24 ?? 8B 44 24", "D9 05 ?? ?? ?? ?? D8 F9 D9 1C ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE F9 D9 5C 24 ?? 89 54 24");

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 87 ?? ?? ?? ?? D9 87 ?? ?? ?? ?? D8 E1 DE CA DE C1 D9 15 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? DE F1 D9 1C ?? E8 ?? ?? ?? ?? 59 D9 1D ?? ?? ?? ?? C7 87", 
		"8B 81 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 59 C3 56", "8B 87 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B7 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 83 C4 ?? 5F C3 83 C4",
		"8B 83 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B3 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? C7 83", "D9 87 ?? ?? ?? ?? D9 87 ?? ?? ?? ?? D8 E1 DE CA DE C1 D9 15 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? DE F1 D9 1C ?? E8 ?? ?? ?? ?? 59 D9 1D ?? ?? ?? ?? 81 C4", 
		"8B 81 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 59 C3 53 83 EC", "8B 85 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05", "8B 85 ?? ?? ?? ?? 57 A3", "8B 87 ?? ?? ?? ?? A3",
		"8B 81 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 59 C3 53 81 EC ?? ?? ?? ?? 8B D9 8B 4B ?? 85 C9 0F 84 ?? ?? ?? ?? 8B 01 8D 54 24");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR4Scan] - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = fAspectRatioScale;

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR1Scan], 6);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR1Scan], [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR2Scan], 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR3Scan], 6);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FADD(fNewAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR4Scan], 6);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR4Scan], [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewAspectRatio2);
			});
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			// These instructions are located in the following functions: ChaseCamera3::switch_to, ChaseCamera3::act, FixedReplayCamera::switch_to, ChaseCamera2::switch_to, AnimatedCamera::switch_to, FPSJeepCamera::switch_to, CMcarctrl::act
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

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1Scan], 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions1MidHook(ctx.edi + 0x90);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2Scan], 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.ecx + 0x90, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3Scan], 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.edi + 0x90, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV4Scan], 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.ebx + 0x120, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV5Scan], 6);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions1MidHook(ctx.edi + 0x90);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6Scan], 6);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.ecx + 0x90, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV7Scan], 6);

			CameraFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV7Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.ebp + 0x90, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV8Scan], 6);

			CameraFOVInstruction8Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV8Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.ebp - 0xAC, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV9Scan], 6);

			CameraFOVInstruction9Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV9Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.edi + 0x90, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV10Scan], 6);

			CameraFOVInstruction10Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV10Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructions2MidHook(ctx.ecx + 0x90, ctx.eax);
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
