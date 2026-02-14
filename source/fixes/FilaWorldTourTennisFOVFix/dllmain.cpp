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
std::string sFixName = "FilaWorldTourTennisFOVFix";
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
float fNewCharacterSelectionHFOV1;
float fNewCharacterSelectionHFOV2;
uint8_t* HUDAspectRatioAddress;
float fNewHUDAspectRatio;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV5;
int iNewCameraFOV6;

// Game detection
enum class Game
{
	FWTT,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	CameraAR,
	CharSelectionAR1,
	CharSelectionAR2,
	HUDAR1,
	HUDAR2,
	HUDAR3,
	HUDAR4
};

enum FOVInstructionsIndices
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	FOV5,
	FOV6
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FWTT, {"Fila World Tour Tennis", "FWTT.exe"}},
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

static SafetyHookMid HUDAspectRatioInstruction1Hook{};
static SafetyHookMid HUDAspectRatioInstruction2Hook{};
static SafetyHookMid HUDAspectRatioInstruction3Hook{};
static SafetyHookMid HUDAspectRatioInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};

enum DestinationInstructions
{
	ECX,
	FMUL,
	FSUB
};

void HUDAspectRatioInstructionsMidHook(uint8_t* address, DestinationInstructions destinationInstruction, SafetyHookContext& ctx)
{
	float& fCurrentHUDAspectRatio = *reinterpret_cast<float*>(address);
	
	fNewHUDAspectRatio = fCurrentHUDAspectRatio * fAspectRatioScale;

	switch (destinationInstruction)
	{
	case ECX:
		ctx.ecx = std::bit_cast<uintptr_t>(fNewHUDAspectRatio);
		break;

	case FMUL:
		FPU::FMUL(fNewHUDAspectRatio);
		break;

	case FSUB:
		FPU::FSUB(fNewHUDAspectRatio);
		break;
	}
}

void FOVFix()
{
	if (eGameType == Game::FWTT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 51 8B 4E ?? 6A", "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 74 ?? 83 F8", "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? DB 44 24", "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8D 44 24", "D8 0D ?? ?? ?? ?? DC C0 D8 25 ?? ?? ?? ?? D8 2D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24", "D8 25 ?? ?? ?? ?? D8 2D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24 ?? DA 35");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CameraAR] - (std::uint8_t*)exeModule);

			spdlog::info("Character Selection Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CharSelectionAR1] - (std::uint8_t*)exeModule);

			spdlog::info("Character Selection Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CharSelectionAR2] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUDAR1] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUDAR2] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUDAR3] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HUDAR4] - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[CameraAR] + 1, fNewAspectRatio);

			fNewCharacterSelectionHFOV1 = 320.0f * fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[CharSelectionAR1] + 4, fNewCharacterSelectionHFOV1);

			fNewCharacterSelectionHFOV2 = 224.0f * fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[CharSelectionAR2] + 4, fNewCharacterSelectionHFOV2);

			HUDAspectRatioAddress = Memory::GetPointerFromAddress<uint32_t>(AspectRatioInstructionsScansResult[HUDAR1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUDAR1], 6);

			HUDAspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUDAR1], [](SafetyHookContext& ctx)
			{
				HUDAspectRatioInstructionsMidHook(HUDAspectRatioAddress, ECX, ctx);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUDAR2], 6);

			HUDAspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUDAR2], [](SafetyHookContext& ctx)
			{
				HUDAspectRatioInstructionsMidHook(HUDAspectRatioAddress, FMUL, ctx);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUDAR3], 6);

			HUDAspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUDAR3], [](SafetyHookContext& ctx)
			{
				HUDAspectRatioInstructionsMidHook(HUDAspectRatioAddress, FSUB, ctx);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[HUDAR4], 6);

			HUDAspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HUDAR4], [](SafetyHookContext& ctx)
			{
				HUDAspectRatioInstructionsMidHook(HUDAspectRatioAddress, FMUL, ctx);
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8B CF E8 ?? ?? ?? ?? D9 5C 24", "68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 44 24", "68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 86", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CE", "D8 42 ?? 8B CE", "DB 40 ?? 51 8B CE D9 1C ?? E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 57");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6] - (std::uint8_t*)exeModule);

			fNewCameraFOV1 = Maths::CalculateNewFOV_DegBased(60.0f, fAspectRatioScale) * fFOVFactor;

			fNewCameraFOV2 = Maths::CalculateNewFOV_DegBased(25.0f, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[FOV1] + 1, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 1, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 1, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4] + 1, fNewCameraFOV2);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV5], 3);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV5 = *reinterpret_cast<float*>(ctx.edx + 0x10);

				fNewCameraFOV5 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV5, fAspectRatioScale);

				FPU::FADD(fNewCameraFOV5);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6], 3);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV6 = *reinterpret_cast<int*>(ctx.eax + 0x14);

				iNewCameraFOV6 = (int)(Maths::CalculateNewFOV_DegBased((float)iCurrentCameraFOV6, fAspectRatioScale) * fFOVFactor);

				FPU::FILD(iNewCameraFOV6);
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