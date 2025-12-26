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
std::string sFixName = "TurokEvolutionFOVFix";
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
float fNewCameraFOV2;
float fNewCameraFOV3;

// Game detection
enum class Game
{
	TE,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	CameraHFOV1Scan,
	CameraVFOV1Scan,
	CameraHFOV2Scan,
	CameraVFOV2Scan,
	CameraFOV1Scan,
	CameraFOV2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TE, {"Turok Evolution", "Turok4.exe"}},
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

static SafetyHookMid CameraHFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraHFOVandVFOVInstructionsMidHook(SafetyHookContext& ctx)
{
	FPU::FDIVR(fNewCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::TE && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 3D ?? ?? ?? ?? C7 44 24 6C 00 00 00 00 C7 44 24 68 00 00 00 00 C7 44 24 5C 00 00 00 00 C7 44 24 58 00 00 00 00", "D8 3D ?? ?? ?? ?? 8B 4C 24 2C 89 4C 24 4C C7 44 24 64 00 00 80 3F C7 44 24 74 00 00 00 00", "D8 3D ?? ?? ?? ?? D9 9E 08 01 00 00 D9 44 24 08", "D8 3D ?? ?? ?? ?? 8B 86 A8 01 00 00 83 C4 04 83 E0 FD 89 86 A8 01 00 00 D9 9E 0C 01 00 00 8B 8E 08 01 00 00", "D9 03 D8 5C 24 14 DF E0 F6 C4 44 7B 38 8A 46 23 84 C0 74 27", "8B 03 50 8B CD E8 51 37 09 00 5F 5E 5D 5B C2 04 00");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraVFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraVFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)exeModule);

			fNewCameraFOV = 1.0f / fAspectRatioScale;

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraHFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraHFOV1Scan], CameraHFOVandVFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraVFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraVFOV1Scan], CameraHFOVandVFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraHFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraHFOV2Scan], CameraHFOVandVFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraVFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraVFOV2Scan], CameraHFOVandVFOVInstructionsMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90", 2);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.ebx);

				if (fCurrentCameraFOV2 != fNewCameraFOV2)
				{
					if (fCurrentCameraFOV2 == 1.5707963268f)
					{
						fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;
					}
					else
					{
						fNewCameraFOV2 = fCurrentCameraFOV2;
					}
				}

				FPU::FLD(fNewCameraFOV2);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90", 2);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(ctx.ebx);

				if (fCurrentCameraFOV3 != fNewCameraFOV3)
				{
					if (fCurrentCameraFOV3 == 1.5707963268f)
					{
						fNewCameraFOV3 = fCurrentCameraFOV3 * fFOVFactor;
					}
					else
					{
						fNewCameraFOV3 = fCurrentCameraFOV3;
					}
				}				

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV3);
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