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
#include <vector>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "GICombatEpisode1BattleOfNormandyWidescreenFix";
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
	GIC,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1Scan,
	Res2Scan
};

enum CameraFOVInstructionsIndices
{
	HFOV1Scan,
	HFOV2Scan,
	HFOV3Scan,
	HFOV4Scan,
	HFOV5Scan,
	HFOV6Scan,
	HFOV7Scan,
	VFOV1Scan,
	VFOV2Scan,
	VFOV3Scan,
	VFOV4Scan,
	VFOV5Scan,
	VFOV6Scan,
	VFOV7Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::GIC, {"G.I. Combat - Episode 1: Battle of Normandy", "GICombat.exe"}},
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

static SafetyHookMid CameraHFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraHFOVInstruction3Hook{};
static SafetyHookMid CameraHFOVInstruction4Hook{};
static SafetyHookMid CameraHFOVInstruction5Hook{};
static SafetyHookMid CameraHFOVInstruction6Hook{};
static SafetyHookMid CameraHFOVInstruction7Hook{};
static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction3Hook{};
static SafetyHookMid CameraVFOVInstruction4Hook{};
static SafetyHookMid CameraVFOVInstruction5Hook{};
static SafetyHookMid CameraVFOVInstruction6Hook{};
static SafetyHookMid CameraVFOVInstruction7Hook{};

enum InstructionType
{
	FLD,
	FMUL,
	EAX
};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float arScale, float fovFactor, InstructionType instructionType, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	fNewCameraFOV = fCurrentCameraFOV * arScale * fovFactor;

	switch (instructionType)
	{
	case FLD:
		FPU::FLD(fNewCameraFOV);
		break;

	case FMUL:
		FPU::FMUL(fNewCameraFOV);
		break;

	case EAX:
		ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
		break;
	}
}

void WidescreenFix()
{
	if (eGameType == Game::GIC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "81 7c 24 ? ? ? ? ? 75 ? 81 7c 24", "c7 05 ? ? ? ? ? ? ? ? c7 05 ? ? ? ? ? ? ? ? b8 ? ? ? ? c3 90 90 cc");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2Scan] - (std::uint8_t*)exeModule);

			// 1024x768
			Memory::Write(ResolutionInstructionsScansResult[Res1Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1Scan] + 14, iCurrentResY);
			
			Memory::Write(ResolutionInstructionsScansResult[Res2Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res2Scan] + 16, iCurrentResY);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "d9 43 ? d8 0d ? ? ? ? d9 43 ? d8 c9 d8 2d", "d8 4b ? d9 1d ? ? ? ? d9 43 ? d8 4b", "d9 43 ? d9 e0 d8 4b ? d9 1d ? ? ? ? d9 43 ? d9 e0 d8 4b", "d9 43 ? d9 e0 d9 1d", "d9 43 ? d8 49 ? d9 41", "d8 4b ? d9 41 ? d8 4b ? d9 41 ? d8 4b ? d9 41 ? d8 4b ? d9 41", "d8 4b ? d9 41 ? d8 4b ? d9 41 ? d8 4b ? d9 41 ? d8 4b ? 8b c3", "d9 43 ? d8 0d ? ? ? ? d9 43 ? d8 c9 d8 05", "d8 4b ? d9 1d ? ? ? ? d9 43 ? d9 e0 d8 4b ? d9 1d ? ? ? ? d9 43 ? d9 e0 d8 4b", "d9 43 ? d9 e0 d8 4b ? d9 1d ? ? ? ? d9 43 ? d9 e0 d9 1d", "8b 43 ? a3 ? ? ? ? 8b 43 ? 83 f8", "d9 43 ? d8 49 ? d9 05", "d8 4b ? d9 41 ? d8 4b ? d9 41 ? d8 4b ? 8b c3", "d8 4b ? d9 41 ? d8 4b ? 8b c3");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV2Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV3Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV4Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV6Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera HFOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV1Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV2Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV3Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV7Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV1Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV2Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV3Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV4Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV5Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV6Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV7Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV1Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV2Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV3Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV4Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV5Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV6Scan], 3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV7Scan], 3);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x70, 1.0f / fAspectRatioScale, 1.0f / fFOVFactor, FLD, ctx);
			});

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x70, 1.0f / fAspectRatioScale, 1.0f / fFOVFactor, FMUL, ctx);
			});

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x70, 1.0f / fAspectRatioScale, 1.0f / fFOVFactor, FLD, ctx);
			});

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x70, 1.0f / fAspectRatioScale, 1.0f / fFOVFactor, FLD, ctx);
			});

			CameraHFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV5Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x68, fAspectRatioScale, fFOVFactor, FLD, ctx);
			});

			CameraHFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV6Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x68, fAspectRatioScale, fFOVFactor, FMUL, ctx);
			});

			CameraHFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV7Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x68, fAspectRatioScale, fFOVFactor, FMUL, ctx);
			});

			CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x74, 1.0f, 1.0f / fFOVFactor, FLD, ctx);
			});

			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x74, 1.0f, 1.0f / fFOVFactor, FMUL, ctx);
			});

			CameraVFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x74, 1.0f, 1.0f / fFOVFactor, FLD, ctx);
			});

			CameraVFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x74, 1.0f, 1.0f / fFOVFactor, EAX, ctx);
			});

			CameraVFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV5Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x6C, 1.0f, fFOVFactor, FLD, ctx);
			});

			CameraVFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV6Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x6C, 1.0f, fFOVFactor, FMUL, ctx);
			});

			CameraVFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV7Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x6C, 1.0f, fFOVFactor, FMUL, ctx);
			});
		}
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		thisModule = hModule;

		Logging();
		Configuration();
		if (DetectGame())
		{
			WidescreenFix();
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