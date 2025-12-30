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
std::string sFixName = "USRacerFOVFix";
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
float fNewCameraFOV;
float fNewCameraHFOV1;
float fNewCameraHFOV2;
float fNewCameraVFOV1;
float fNewCameraVFOV2;
float fNewCameraHFOV6;

// Game detection
enum class Game
{
	USR,
	Unknown
};

enum CameraHFOVInstructionsIndices
{
	CameraHFOV1Scan,
	CameraHFOV2Scan,
	CameraHFOV3Scan,
	CameraHFOV4Scan,
	CameraHFOV5Scan,
	CameraHFOV6Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::USR, {"US Racer", "USARacer.exe"}},
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
static SafetyHookMid CameraHFOVInstruction3Hook{};
static SafetyHookMid CameraHFOVInstruction4Hook{};
static SafetyHookMid CameraHFOVInstruction5Hook{};
static SafetyHookMid CameraHFOVInstruction6Hook{};

enum InstructionType
{
	FLD,
	FSUB,
	FADD
};

void CameraFOVInstructionsMidHook(uintptr_t CameraFOVAddress, InstructionType DestinationInstruction)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

	fNewCameraFOV = fCurrentCameraFOV * fAspectRatioScale;

	switch (DestinationInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewCameraFOV);
			break;
		}

		case FSUB:
		{
			FPU::FSUB(fNewCameraFOV);
			break;
		}

		case FADD:
		{
			FPU::FADD(fNewCameraFOV);
			break;
		}
	}
}

void FOVFix()
{
	if ((eGameType == Game::USR) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraHFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 43 ?? D8 23", "D9 43 ?? D8 03", "d9 81 ? ? ? ? d8 a1 ? ? ? ? d8 3d ? ? ? ? d9 81", "d9 81 ? ? ? ? d8 81 ? ? ? ? 89 44 24", "d9 86 ? ? ? ? d9 c0 d8 4b", "8b 86 ? ? ? ? 57 89 44 24 ? 8b 51 ? 8d 4c 24 ? 89 54 24 ? 51 8b d0 53");
		if (Memory::AreAllSignaturesValid(CameraHFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV6Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV1Scan], "\x90\x90\x90\x90\x90", 5);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x4, FLD);

				CameraFOVInstructionsMidHook(ctx.ebx, FSUB);
			});

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV2Scan], "\x90\x90\x90\x90\x90", 5);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x4, FLD);

				CameraFOVInstructionsMidHook(ctx.ebx, FADD);
			});

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV3Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x150, FLD);

				CameraFOVInstructionsMidHook(ctx.ecx + 0x14C, FSUB);
			});

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV4Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x14C, FLD);

				CameraFOVInstructionsMidHook(ctx.ecx + 0x150, FADD);
			});
			
			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction5Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV5Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x14C, FLD);
			});

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction6Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV6Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV6 = *reinterpret_cast<float*>(ctx.esi + 0x150);

				fNewCameraHFOV6 = fCurrentCameraHFOV6 * fAspectRatioScale;

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraHFOV6);
			});
		}

		std::uint8_t* CameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 99 ?? ?? ?? ?? 85 C9");
		if (CameraFOVInstructionsScanResult)
		{
			spdlog::info("Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			fNewCameraHFOV1 = -0.5f * fFOVFactor;

			fNewCameraHFOV2 = 0.5f * fFOVFactor;

			fNewCameraVFOV1 = 0.375f * fFOVFactor;

			fNewCameraVFOV2 = -0.375f * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScanResult + 6, fNewCameraHFOV1);

			Memory::Write(CameraFOVInstructionsScanResult + 24, fNewCameraHFOV2);

			Memory::Write(CameraFOVInstructionsScanResult + 34, fNewCameraVFOV1);

			Memory::Write(CameraFOVInstructionsScanResult + 44, fNewCameraVFOV2);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instructions scan memory address.");
			return;
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
			FOVFix();
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