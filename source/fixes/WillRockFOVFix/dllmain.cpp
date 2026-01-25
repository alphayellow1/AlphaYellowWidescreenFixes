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
#include <cmath> // For atanf, tanf
#include <unordered_set>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "WilLRockFOVFix";
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
constexpr float fDefaultCameraHFOV = 75.0f;
constexpr float fDefaultCameraVFOV = 59.84044647f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV1;
float fNewCameraVFOV1;
float fNewCameraHFOV2;
float fNewCameraVFOV2;
float fNewHUDPosition2;
float fNewHUDPosition3;
float fNewHorizontalRes;

// Game detection
enum class Game
{
	WR,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	CameraHFOV1Scan,
	CameraVFOV1Scan,
	CameraHFOV2Scan,
	CameraVFOV2Scan,
};

enum HUDInstructionsIndices
{
	HUD1Scan,
	HUD2Scan,
	HUD3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WR, {"Will Rock", "WilLRock.exe"}},
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
static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};
static SafetyHookMid HUDInstruction1Hook{};
static SafetyHookMid HUDInstruction2Hook{};
static SafetyHookMid HUDInstruction3Hook{};

void FOVFix()
{
	if (eGameType == Game::WR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 81 ?? ?? ?? ?? 8B 44 24 ?? 89 81 ?? ?? ?? ?? B8 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D8 4C 24", "D9 81 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 89 81", "8B 90 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 89 54 24", "8B 88 ?? ?? ?? ?? 89 54 24 ?? 89 4C 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraVFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraHFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraVFOV2Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CameraHFOV1Scan], 6);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraHFOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.ecx + 0x14C);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.ecx + 0x150);

				if (Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV) && (Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV) || Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV / fAspectRatioScale)))
				{
					fNewCameraHFOV1 = Maths::CalculateNewHFOV_DegBased(fCurrentCameraHFOV, fAspectRatioScale, fFOVFactor);
				}
				else
				{
					fNewCameraHFOV1 = Maths::CalculateNewHFOV_DegBased(fCurrentCameraHFOV, fAspectRatioScale);
				}

				FPU::FLD(fNewCameraHFOV1);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CameraVFOV1Scan], 6);

			CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraVFOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.ecx + 0x14C);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.ecx + 0x150);

				if (Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV) && (Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV) || Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV / fAspectRatioScale)))
				{
					fNewCameraVFOV1 = Maths::CalculateNewVFOV_DegBased(fCurrentCameraVFOV, fFOVFactor);
				}
				else
				{
					fNewCameraVFOV1 = fCurrentCameraVFOV;
				}

				FPU::FLD(fNewCameraVFOV1);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CameraHFOV2Scan], 6);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraHFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax + 0x14C);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x150);

				if (Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV) && (Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV) || Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV / fAspectRatioScale)))
				{
					fNewCameraHFOV2 = Maths::CalculateNewHFOV_DegBased(fCurrentCameraHFOV, fAspectRatioScale, fFOVFactor);
				}
				else
				{
					fNewCameraHFOV2 = Maths::CalculateNewHFOV_DegBased(fCurrentCameraHFOV, fAspectRatioScale);
				}

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraHFOV2);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CameraVFOV2Scan], 6);

			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraVFOV2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax + 0x14C);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.eax + 0x150);

				if (Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV) && (Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV) || Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV / fAspectRatioScale)))
				{
					fNewCameraVFOV2 = Maths::CalculateNewVFOV_DegBased(fCurrentCameraVFOV, fFOVFactor);
				}
				else
				{
					fNewCameraVFOV2 = fCurrentCameraVFOV;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraVFOV2);
			});
		}

		std::vector<std::uint8_t*> HUDInstructionsScansResult = Memory::PatternScan(exeModule, "d8 b6 ? ? ? ? d9 54 24 ? d9 9e ? ? ? ? db 86", "d9 41 ?? d8 41 ?? d8 0d ?? ?? ?? ?? d9 5c 24", "d9 80 ? ? ? ? d9 80 ? ? ? ? 8b 41");
		if (Memory::AreAllSignaturesValid(HUDInstructionsScansResult) == true)
		{
			spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), HUDInstructionsScansResult[HUD3Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(HUDInstructionsScansResult[HUD1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			HUDInstruction1Hook = safetyhook::create_mid(HUDInstructionsScansResult[HUD1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentHorizontalRes = *reinterpret_cast<float*>(ctx.esi + 0xEC);

				fNewHorizontalRes = fCurrentHorizontalRes * fAspectRatioScale;

				FPU::FDIV(fNewHorizontalRes);
			});

			Memory::PatchBytes(HUDInstructionsScansResult[HUD2Scan], "\x90\x90\x90", 3);

			HUDInstruction2Hook = safetyhook::create_mid(HUDInstructionsScansResult[HUD2Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDPosition2 = *reinterpret_cast<float*>(ctx.ecx + 0x5C);

				fNewHUDPosition2 = fCurrentHUDPosition2 / fAspectRatioScale;

				FPU::FLD(fNewHUDPosition2);
			});

			Memory::PatchBytes(HUDInstructionsScansResult[HUD3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			HUDInstruction3Hook = safetyhook::create_mid(HUDInstructionsScansResult[HUD3Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDPosition3 = *reinterpret_cast<float*>(ctx.eax + 0xFC);

				fNewHUDPosition3 = fCurrentHUDPosition3 * fAspectRatioScale;

				FPU::FLD(fNewHUDPosition3);
			});
		}

		// 0f 85 ? ? ? ? 8d 44 24 ? 53 8b 1d

		// 8b 86 ? ? ? ? d8 b6

		// c7 44 24 ? ? ? ? ? 89 7c 24 ? 8b 81

		// a1 ? ? ? ? 56 57 8d 71

		// 89 08 89 50 ? 83 c4

		// 68 ? ? ? ? e8 ? ? ? ? 8b 0d ? ? ? ? e8 ? ? ? ? 6a ? 6a ? 68 ? ? ? ? b9 ? ? ? ? e8 ? ? ? ? 8b 96 ? ? ? ? 8b 82
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
