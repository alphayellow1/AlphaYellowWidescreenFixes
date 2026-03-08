// Include necessary headers
#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>
#include <vector>
#include <algorithm>
#include <map>
#include <windows.h>
#include <psapi.h> // For GetModuleInformation
#include <fstream>
#include <filesystem>
#include <cmath> // For atanf, tanf
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "KookaBongaFOVFix";
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
	KB,
	Unknown
};

enum CameraFOVInstructionsIndices
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
	{Game::KB, {"Kooka Bonga", "Kookabonga.exe"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};

enum AngleMode
{
	DEG,
	RAD
};

enum DestInstruction
{
	FLD,
	EAX,
	EDX,
	ECX
};

void CameraFOVInstructionsMidHook(uintptr_t CameraFOVAddress, AngleMode angleMode, float fovFactor, DestInstruction destInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = Memory::ReadMem(CameraFOVAddress);

	if (angleMode == DEG)
	{
		fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fovFactor;
	}
	else if (angleMode == RAD)
	{
		fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fovFactor;
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

		case EDX:
		{
			ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}

		case ECX:
		{
			ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}
	}
}

void FOVFix()
{
	if (eGameType == Game::KB && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 40 08 D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D9 5E 78",
		"D9 83 80 00 00 00 D8 0D ?? ?? ?? ?? 8B 44 24 10 50 51 D8 0D ?? ?? ?? ?? D9 F2", "8B 86 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 6A", "8B 42 ?? 51 50 E8 ?? ?? ?? ?? 8B 56",
		"8B 96 ?? ?? ?? ?? 51 52 E8 ?? ?? ?? ?? 8B 4E", "8B 4C 24 ?? 50 51 E8 ?? ?? ?? ?? 6A");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 3);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax + 0x8, DEG, fFOVFactor, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x80, DEG, fFOVFactor, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3], 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x1C4, RAD, fFOVFactor, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV4], 3);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.edx + 0x48, RAD, fFOVFactor, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV5], 6);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x18C, RAD, 1.0f, EDX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6], 4);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x18, RAD, 1.0f, ECX, ctx);
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