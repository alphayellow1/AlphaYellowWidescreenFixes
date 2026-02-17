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
std::string sFixName = "TheDevilInsideFOVFix";
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
float fNewDaveCameraFOV;
float fNewDevaCameraFOV;
float fNewCameraFOV4;
float fNewCameraFOV5;
uint8_t* CameraFOV6Address;
double dNewCameraFOV6;
float fNewGeneralFOV;

// Game detection
enum class Game
{
	TDI,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	FOV5,
	FOV6,
	FOV7,
	FOV8
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TDI, {"The Devil Inside", "Devil.exe"}},
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

static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};
static SafetyHookMid CameraFOVInstruction7Hook{};
static SafetyHookMid GeneralCameraFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::TDI && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "B8 ?? ?? ?? ?? 89 4E ?? 89 86 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 44 24", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 3D ?? ?? ?? ?? 89 B1", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 35 ?? ?? ?? ?? 89 B2", "8B 83 ?? ?? ?? ?? 89 83", "8B 8B ?? ?? ?? ?? 89 8B", "DD 05 ?? ?? ?? ?? D8 4B ?? D8 83", "DD 05 ?? ?? ?? ?? D8 4B ?? D9 E0", "D9 81 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D9 C9");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV7] - (std::uint8_t*)exeModule);

			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV8] - (std::uint8_t*)exeModule);

			fNewDaveCameraFOV = 76.0f * fFOVFactor;

			fNewDevaCameraFOV = 67.0f * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[FOV1] + 1, fNewDaveCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 6, fNewDaveCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 16, fNewDevaCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 6, fNewDevaCameraFOV);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3] + 16, fNewDaveCameraFOV);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV4], 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV4 = *reinterpret_cast<float*>(ctx.ebx + 0xC7);

				fNewCameraFOV4 = fCurrentCameraFOV4 * fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV4);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV5], 6);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV5 = *reinterpret_cast<float*>(ctx.ebx + 0xC7);

				fNewCameraFOV5 = fCurrentCameraFOV5 * fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV5);
			});

			CameraFOV6Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[FOV6] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6], 6);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6], [](SafetyHookContext& ctx)
			{
				double& dCurrentCameraFOV6 = *reinterpret_cast<double*>(CameraFOV6Address);

				dNewCameraFOV6 = dCurrentCameraFOV6 * (double)fFOVFactor;

				FPU::FLD(dNewCameraFOV6);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV7], 6);

			CameraFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV7], [](SafetyHookContext& ctx)
			{
				FPU::FLD(dNewCameraFOV6);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV8], 6); // NOP out the original instruction

			GeneralCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV8], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = *reinterpret_cast<float*>(ctx.ecx + 0xC0);

				fNewGeneralFOV = Maths::CalculateNewFOV_RadBased(fCurrentGeneralFOV, fAspectRatioScale);

				FPU::FLD(fNewGeneralFOV);
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