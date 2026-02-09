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
std::string sFixName = "SuperbikesRidingChallengeFOVFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHUDAspectRatio;
uint8_t* BikeSelectionFOVAddress2;
float fNewCameraFOV;

// Game detection
enum class Game
{
	SBRC,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	BikeSelectionAR,
	HUDAR
};

enum CameraFOVInstructionsIndices
{
	BikeSelectionFOV1,
	BikeSelectionFOV2,
	RacesFOV1,
	RacesFOV2,
	FOVReference
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SBRC, {"Super-bikes Riding Challenge", "SBike.exe"}},
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
static SafetyHookMid BikeSelectionFOVInstruction1Hook{};
static SafetyHookMid BikeSelectionFOVInstruction2Hook{};
static SafetyHookMid RacesCameraFOVInstruction1Hook{};
static SafetyHookMid RacesCameraFOVInstruction2Hook{};

enum destInstruction
{
	FLD,
	EAX
};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, destInstruction destInstruction, bool isGeneralFOV, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	if (isGeneralFOV == true)
	{
		fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale);
	}
	else
	{
		fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, 1.0f / fAspectRatioScale) * fFOVFactor;
	}

	switch (destInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewCameraFOV);
			break;
		}

		case EAX:
		{
			ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}		
	}
}

void FOVFix()
{
	if (eGameType == Game::SBRC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "c7 46 ? ? ? ? ? c7 46 ? ? ? ? ? c7 46 ? ? ? ? ? a1", "8B 54 24 ?? D8 74 24 ?? 89 54 24");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Bike Selection Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[BikeSelectionAR] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUDAR] - (std::uint8_t*)exeModule);
			
			Memory::Write(AspectRatioInstructionsScansResult[BikeSelectionAR] + 3, fNewAspectRatio);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUDAR], 4);

			HUDAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUDAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDAspectRatio = *reinterpret_cast<float*>(ctx.esp + 0x3C);

				fNewHUDAspectRatio = fCurrentHUDAspectRatio * fAspectRatioScale;

				ctx.edx = std::bit_cast<uintptr_t>(fNewHUDAspectRatio);
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8b 44 24 ? 50 50 e8 ? ? ? ? 5f 5d 5b 5e c2", "a1 ? ? ? ? 50 50", "8b 44 24 ? 50 50 e8 ? ? ? ? a0", "d9 07 d8 0d ? ? ? ? 51", "d9 44 24 ? 56 d8 0d ? ? ? ? 8b f1 8b 4e");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Bike Selection FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[BikeSelectionFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Bike Selection FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[BikeSelectionFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Races FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[RacesFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Races FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[RacesFOV2] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[BikeSelectionFOV1], 4);

			BikeSelectionFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[BikeSelectionFOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x18, EAX, true, ctx);
			});

			BikeSelectionFOVAddress2 = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[BikeSelectionFOV2] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[BikeSelectionFOV2], 5);

			BikeSelectionFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[BikeSelectionFOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook((uintptr_t)BikeSelectionFOVAddress2, EAX, true, ctx);
			});
			
			Memory::WriteNOPs(CameraFOVInstructionsScansResult[RacesFOV1], 4);

			RacesCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[RacesFOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x8, EAX, true, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[RacesFOV2], 2);

			RacesCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[RacesFOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.edi, FLD, false, ctx);
			});

			static SafetyHookMid fovhook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOVReference], [](SafetyHookContext& ctx)
			{
				float& fov = *(float*)(ctx.esp + 0xC);

				spdlog::info("[Hook] Raw incoming FOV: {:.12f}", fov);
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