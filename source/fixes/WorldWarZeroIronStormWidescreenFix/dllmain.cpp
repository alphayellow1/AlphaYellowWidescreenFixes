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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "WorldWarZeroIronStormWidescreenFix";
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
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
uint8_t* CameraFOV3Address;

// Game detection
enum class Game
{
	WWZIS,
	Unknown
};

enum ResolutionsInstructionsIndices
{
	Res1,
	Res2,
	Res3,
	Res4,
	Res5,
	Res6
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2,
	FOV3
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WWZIS, {"World War Zero: Iron Storm", "WorldWarZero.exe"}},
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

static SafetyHookMid ResolutionInstructions1Hook{};
static SafetyHookMid ResolutionInstructions2Hook{};
static SafetyHookMid ResolutionInstructions3Hook{};
static SafetyHookMid ResolutionInstructions4Hook{};
static SafetyHookMid ResolutionInstructions5Hook{};
static SafetyHookMid ResolutionInstructions6Hook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};

void WidescreenFix()
{
	if (eGameType == Game::WWZIS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 8A D8", "8B 54 24 ?? 8B 44 24 ?? A3", "8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 53", "8B 15 ?? ?? ?? ?? A1 ?? ?? ?? ?? 68", "89 1E 89 6E", "8B 08 8B 50 ?? 8B 40 ?? 83 C4 ?? 89 7C 24");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res5] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res6] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res1], 12);

			ResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res2], 8);

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res3], 11);

			ResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res3], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res4], 11);

			ResolutionInstructions4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res5], 5);

			ResolutionInstructions5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res5], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0x4) = iCurrentResY;
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res6], 5);

			ResolutionInstructions6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res6], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 74 06 D9 C9 D9 E0 D9 C9");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionScanResult + 2, &fNewAspectRatio);
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 44 24 ?? D8 0D ?? ?? ?? ?? D9 58", "D9 41 ?? D8 B6", "D8 0D ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? 83 C4 ?? C3");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 4);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.esp + 0x4);

				fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, fAspectRatioScale);

				FPU::FLD(fNewCameraFOV1);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 3);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x4C);

				fNewCameraFOV2 = fCurrentCameraFOV2 / fFOVFactor;

				FPU::FLD(fNewCameraFOV2);
			});

			CameraFOV3Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV3] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3], 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(CameraFOV3Address);

				fNewCameraFOV3 = fCurrentCameraFOV3 * fFOVFactor;

				FPU::FMUL(fNewCameraFOV3);
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