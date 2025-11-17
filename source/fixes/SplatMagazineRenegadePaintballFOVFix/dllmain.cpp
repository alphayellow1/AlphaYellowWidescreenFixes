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
std::string sFixName = "SplatMagazineRenegadePaintballFOVFix";
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
float fCameraFOVFactor;
float fWeaponFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewBriefingCameraFOV;
float fNewHipfireCameraFOV;
float fNewCameraZoomFOV;
float fNewWeaponFOV;
uint8_t* CameraZoomAddress;

// Game detection
enum class Game
{
	SMRP,
	Unknown
};

enum CameraFOVInstructionsIndex
{
	BriefingCameraFOVScan,
	HipfireCameraFOV1Scan,
	HipfireCameraFOV2Scan,
	CameraZoomScan,
	WeaponFOVScan
};

enum AspectRatioInstructionsIndex
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan,
	AspectRatio4Scan,
	AspectRatio5Scan,
	AspectRatio6Scan,
	AspectRatio7Scan,
	AspectRatio8Scan,
	AspectRatio9Scan,
	AspectRatio10Scan,
	AspectRatio11Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SMRP, {"SPLAT Magazine: Renegade Paintball", "PaintballGame.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", fCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", fWeaponFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fCameraFOVFactor);
	spdlog_confparse(fWeaponFOVFactor);

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

static SafetyHookMid BriefingCameraFOVInstructionHook{};
static SafetyHookMid HipfireCameraFOVInstruction1Hook{};
static SafetyHookMid HipfireCameraFOVInstruction2Hook{};
static SafetyHookMid WeaponFOVInstructionHook{};
static SafetyHookMid CameraZoomFOVInstructionHook{};

void HipfireCameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentHipfireCameraFOV = *reinterpret_cast<float*>(ctx.ebx + 0x19C);

	fNewHipfireCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentHipfireCameraFOV, fAspectRatioScale) * fCameraFOVFactor;

	FPU::FLD(fNewHipfireCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::SMRP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "89 48 58 8B 4A 5C 89 48 5C 8B 4A 60 89 48 60 8D 72 64", "D9 83 9C 01 00 00 8D 44 24 14", "D9 83 9C 01 00 00 50 D8 E1 51", "D9 05 ?? ?? ?? ?? D8 64 24 5C 5F D8 4C 24 68", "8B 48 04 33 C0 89 44 24 10 89 44 24 14 89 44 24 18 89 44 24 1C 89 44 24 20 89 44 24 24 89 44 24 28 89 44 24 2C 89 44 24 30 89 44 24 34 89 44 24 38 89 44 24 3C");

		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Briefing Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[BriefingCameraFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire Camera FOV Instruction 1: Address is{:s} + {:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HipfireCameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HipfireCameraFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera Zoom FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraZoomScan] - (std::uint8_t*)exeModule);

			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[WeaponFOVScan] - (std::uint8_t*)exeModule);

			// Briefing Camera FOV
			Memory::PatchBytes(CameraFOVInstructionsScansResult[BriefingCameraFOVScan], "\x90\x90\x90", 3);

			BriefingCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[BriefingCameraFOVScan], [](SafetyHookContext& ctx)
			{
				float fCurrentBriefingCameraFOV = std::bit_cast<float>(ctx.ecx);

				if (fCurrentBriefingCameraFOV == 0.5142856836f) // 29.5 degrees in radians
				{
					fNewBriefingCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentBriefingCameraFOV, fAspectRatioScale);
				}
				else
				{
					fNewBriefingCameraFOV = fCurrentBriefingCameraFOV;
				}

				*reinterpret_cast<float*>(ctx.eax + 0x58) = fNewBriefingCameraFOV;
			});

			// Hipfire Camera FOV 1
			Memory::PatchBytes(CameraFOVInstructionsScansResult[HipfireCameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			HipfireCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireCameraFOV1Scan], HipfireCameraFOVInstructionMidHook);

			// Hipfire Camera FOV 2
			Memory::PatchBytes(CameraFOVInstructionsScansResult[HipfireCameraFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			HipfireCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HipfireCameraFOV2Scan], HipfireCameraFOVInstructionMidHook);

			// Camera Zoom FOV
			CameraZoomAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[CameraZoomScan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraZoomScan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraZoomFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraZoomScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraZoomFOV = *reinterpret_cast<float*>(CameraZoomAddress);

				fNewCameraZoomFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraZoomFOV, fAspectRatioScale);

				FPU::FLD(fNewCameraZoomFOV);
			});

			// Weapon FOV
			Memory::PatchBytes(CameraFOVInstructionsScansResult[WeaponFOVScan], "\x90\x90\x90", 3);

			WeaponFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[WeaponFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentWeaponFOV = *reinterpret_cast<float*>(ctx.eax + 0x4);

				fNewWeaponFOV = Maths::CalculateNewFOV_RadBased(fCurrentWeaponFOV, fAspectRatioScale) * fWeaponFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewWeaponFOV);
			});
		}

		std::vector<std::uint8_t*> AspectRatioScansResult = Memory::PatternScan(exeModule, "C7 40 64 AB AA AA 3F DD 05 B0 B7 8A", "00 C7 43 64 AB AA AA 3F 8B 80 00 01 00", "14 C7 42 64 AB AA AA 3F 8B 06 8B 54", "00 C7 41 64 AB AA AA 3F EB 16 A1 64 6C A0", "24 8C 00 00 00 AB AA AA 3F C6 84 24", "64 FC FF C7 40 64 AB AA AA 3F DD 05 F0 B4 8A", "4C C7 41 3C AB AA AA 3F 8B 55 4C 8D 86 EC", "00 C7 40 3C AB AA AA 3F 8B 45 50 8B 54 24", "74 C7 43 3C AB AA AA 3F D8 0D 38 B0 8A", "C7 42 3C AB AA AA 3F 8B C8 89 4A 48 89 4A", "6C 16 AD 3F AB AA AA 3F 35 58 A8 3F");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio1Scan] + 3 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio2Scan] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio3Scan] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio4Scan] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio5Scan] + 5 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 6: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio6Scan] + 6 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 7: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio7Scan] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 8: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio8Scan] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 9: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio9Scan] + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 10: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio10Scan] + 3 - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 11: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AspectRatio11Scan] + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScansResult[AspectRatio1Scan] + 3, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio2Scan] + 4, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio3Scan] + 4, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio4Scan] + 4, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio5Scan] + 5, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio6Scan] + 6, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio7Scan] + 4, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio8Scan] + 4, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio9Scan] + 4, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio10Scan] + 3, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AspectRatio11Scan] + 4, fNewAspectRatio);
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