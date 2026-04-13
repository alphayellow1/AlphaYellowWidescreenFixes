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
std::string sFixName = "StarTrekBridgeCommanderFOVFix";
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
constexpr float fOriginalHFOV1 = 0.5f;
constexpr float fOriginalHFOV2 = -0.5f;
constexpr float fOriginalVFOV1 = 0.375f;
constexpr float fOriginalVFOV2 = -0.375f;

// Ini variables
bool bFixActive;
float fFOVFactor;

// Variables
int iCurrentWidth;
int iCurrentHeight;
float fNewAspectRatio;
float fAspectRatioScale;
float fNewInteriorHFOV1;
float fNewInteriorHFOV2;
float fNewInteriorVFOV1;
float fNewInteriorVFOV2;
uintptr_t ExteriorFOVAddress;
float fNewExteriorFOV;

// Game detection
enum class Game
{
	STBC,
	Unknown
};

enum SpaceshipInteriorFOVInstructionsIndex
{
	InteriorHFOV1,
	InteriorHFOV2,
	InteriorHFOV3,
	InteriorHFOV4,
	InteriorHFOV5,
	InteriorHFOV6,
	InteriorVFOV1,
	InteriorVFOV2,
	InteriorVFOV3,
	InteriorVFOV4,
	InteriorVFOV5,
	InteriorVFOV6,
	ExteriorHFOV1,
	ExteriorHFOV2,
	ExteriorVFOV,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::STBC, {"Star Trek: Bridge Commander", "stbc.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(fFOVFactor);

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

static SafetyHookMid ResolutionWidthInstructionHook{};
static SafetyHookMid ResolutionHeightInstructionHook{};
static SafetyHookMid SpaceshipExteriorHFOVInstruction1Hook{};
static SafetyHookMid SpaceshipExteriorHFOVInstruction2Hook{};
static SafetyHookMid SpaceshipExteriorVFOVInstructionHook{};

enum DestInstruction
{
	FLD,
	FMUL,
	EAX
};

void SpaceshipExteriorFOVMidHooks(uintptr_t FOVAddress, DestInstruction destInstruction, SafetyHookContext& ctx)
{
	float& fCurrentExteriorFOV = Memory::ReadMem(FOVAddress);

	fNewExteriorFOV = fCurrentExteriorFOV * fAspectRatioScale * fFOVFactor;

	switch (destInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewExteriorFOV);
			break;
		}

		case FMUL:
		{
			FPU::FMUL(fNewExteriorFOV);
			break;
		}
	
		case EAX:
		{
			ctx.eax = std::bit_cast<uintptr_t>(fNewExteriorFOV);
			break;
		}
	}
}

void FOVFix()
{
	if (eGameType == Game::STBC && bFixActive == true)
	{
		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "8B 94 24 ?? ?? ?? ?? 52 8B 94 24 ?? ?? ?? ?? 52 51");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			ResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 8, [](SafetyHookContext& ctx)
			{
				iCurrentWidth = *(int*)(ctx.esp + 0x5D8);

				fNewAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

				fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

				fNewInteriorHFOV1 = fOriginalHFOV1 * fAspectRatioScale * fFOVFactor;

				fNewInteriorHFOV2 = fOriginalHFOV2 * fAspectRatioScale * fFOVFactor;
			});

			ResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				iCurrentHeight = *(int*)(ctx.esp + 0x5D8);				
			});
		}
		else
		{
			spdlog::error("Failed to find resolution instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> SpaceshipFOVInstructionsScansResult = Memory::PatternScan(exeModule, 
		/*Interior HFOV*/
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4", 
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4",
		"D8 0D ?? ?? ?? ?? C7 84 24", "D8 0D ?? ?? ?? ?? 8B 8E", "D8 0D ?? ?? ?? ?? 8B 89",
		/*Interior VFOV*/
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 83 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 4C 24",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 86",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D9 5C 24 ?? D9 05 ?? ?? ?? ?? D8 5C 24 ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 44 24 ?? D8 15 ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? DD D8 D9 05 ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ?? D9 99 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 89 91 ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4",
		/*Exterior FOV*/
		"A1 ?? ?? ?? ?? 5E 89 44 24 ?? D8 7C 24", "D9 05 ?? ?? ?? ?? D9 E0 D9 5C 24 ?? D8 0D", "D8 0D ?? ?? ?? ?? D9 54 24 ?? D9 E0");
		if (Memory::AreAllSignaturesValid(SpaceshipFOVInstructionsScansResult) == true)
		{
			spdlog::info("Spaceship Interior HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorHFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorHFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorHFOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorHFOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorHFOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior HFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorHFOV6] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorVFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorVFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorVFOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorVFOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorVFOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Interior VFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[InteriorVFOV6] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Exterior HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[ExteriorHFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Exterior HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[ExteriorHFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Spaceship Exterior VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), SpaceshipFOVInstructionsScansResult[ExteriorVFOV] - (std::uint8_t*)exeModule);

			fNewInteriorHFOV1 = fOriginalHFOV1 * fAspectRatioScale * fFOVFactor;

			fNewInteriorHFOV2 = fOriginalHFOV2 * fAspectRatioScale * fFOVFactor;

			fNewInteriorVFOV1 = fOriginalVFOV1 * fFOVFactor;

			fNewInteriorVFOV2 = fOriginalVFOV2 * fFOVFactor;

			Memory::Write(SpaceshipFOVInstructionsScansResult, InteriorHFOV1, InteriorHFOV3, 2, &fNewInteriorHFOV1);

			Memory::Write(SpaceshipFOVInstructionsScansResult, InteriorHFOV4, InteriorHFOV6, 2, &fNewInteriorHFOV2);

			Memory::Write(SpaceshipFOVInstructionsScansResult, InteriorVFOV1, InteriorVFOV3, 2, &fNewInteriorVFOV1);

			Memory::Write(SpaceshipFOVInstructionsScansResult, InteriorVFOV4, InteriorVFOV6, 2, &fNewInteriorVFOV2);

			ExteriorFOVAddress = Memory::GetPointerFromAddress(SpaceshipFOVInstructionsScansResult[ExteriorHFOV1] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(SpaceshipFOVInstructionsScansResult[ExteriorHFOV1], 5);

			Memory::WriteNOPs(SpaceshipFOVInstructionsScansResult, ExteriorHFOV2, ExteriorVFOV, 0, 6);

			SpaceshipExteriorHFOVInstruction1Hook = safetyhook::create_mid(SpaceshipFOVInstructionsScansResult[ExteriorHFOV1], [](SafetyHookContext& ctx)
			{
				SpaceshipExteriorFOVMidHooks(ExteriorFOVAddress, EAX, ctx);
			});

			SpaceshipExteriorHFOVInstruction2Hook = safetyhook::create_mid(SpaceshipFOVInstructionsScansResult[ExteriorHFOV2], [](SafetyHookContext& ctx)
			{
				SpaceshipExteriorFOVMidHooks(ExteriorFOVAddress, FLD, ctx);
			});

			SpaceshipExteriorVFOVInstructionHook = safetyhook::create_mid(SpaceshipFOVInstructionsScansResult[ExteriorVFOV], [](SafetyHookContext& ctx)
			{
				SpaceshipExteriorFOVMidHooks(ExteriorFOVAddress, FMUL, ctx);
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