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
float fCameraFOVFactor;
float fWeaponFOVFactor;
float fZoomFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
uintptr_t CameraZoomAddress;

// Game detection
enum class Game
{
	SMRP,
	Unknown
};

enum CameraFOVInstructionsIndex
{
	Briefing,
	Hipfire1,
	Hipfire2,
	Zoom,
	Weapon
};

enum AspectRatioInstructionsIndex
{
	AR1,
	AR2,
	AR3,
	AR4,
	AR5,
	AR6,
	AR7,
	AR8,
	AR9,
	AR10,
	AR11
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
	inipp::get_value(ini.sections["Settings"], "ZoomFactor", fZoomFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fCameraFOVFactor);
	spdlog_confparse(fWeaponFOVFactor);
	spdlog_confparse(fZoomFactor);

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

enum DestInstruction
{
	FLD,
	ECX
};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float fovFactor, DestInstruction destInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

	fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fovFactor;

	switch (destInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewCameraFOV);
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
	if (eGameType == Game::SMRP && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 40 ?? 8B 86 ?? ?? ?? ?? D9 50", "D9 83 ?? ?? ?? ?? 8D 44 24 ?? D9 83 ?? ?? ?? ?? 50",
			"D9 83 ?? ?? ?? ?? 50", "D9 05 ?? ?? ?? ?? D8 64 24 ?? 5F", "8B 48 ?? 33 C0 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 8B 44 F2");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Briefing Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Briefing] - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire Camera FOV Instruction 1: Address is{:s} + {:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire1] - (std::uint8_t*)exeModule);

			spdlog::info("Hipfire Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera Zoom FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Zoom] - (std::uint8_t*)exeModule);

			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Weapon] - (std::uint8_t*)exeModule);

			// Briefing Camera FOV
			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Briefing], 3);

			BriefingCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Briefing], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax + 0x4, 1.0f, FLD, ctx);
			});

			// Hipfire Camera FOV 1
			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Hipfire1], 6);

			HipfireCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Hipfire1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x19C, fCameraFOVFactor, FLD, ctx);
			});

			// Hipfire Camera FOV 2
			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Hipfire2], 6);

			HipfireCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Hipfire2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x19C, fCameraFOVFactor, FLD, ctx);
			});

			// Camera Zoom FOV
			CameraZoomAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[Zoom] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Zoom], 6);

			CameraZoomFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraZoomAddress, 1.0f / fZoomFactor, FLD, ctx);
			});

			// Weapon FOV
			Memory::WriteNOPs(CameraFOVInstructionsScansResult[Weapon], 3);

			WeaponFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Weapon], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax + 0x4, fWeaponFOVFactor, ECX, ctx);
			});
		}

		std::vector<std::uint8_t*> AspectRatioScansResult = Memory::PatternScan(exeModule, "C7 40 ?? ?? ?? ?? ?? DD 05 ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? D9 F2 50 DD D8 D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FE D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FF D9 98 ?? ?? ?? ?? C7 40",
		"C7 43 ?? ?? ?? ?? ?? 8B 80", "C7 42 ?? ?? ?? ?? ?? 8B 06 8B 54 24", "C7 41 ?? ?? ?? ?? ?? EB ?? A1", "C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? C6 84 24 ?? ?? ?? ?? ?? C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? E8",
		"C7 40 ?? ?? ?? ?? ?? DD 05 ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? D9 F2 50 DD D8 D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FE D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FF D9 98 ?? ?? ?? ?? 8B C5", "C7 41 ?? ?? ?? ?? ?? 8B 55 ?? 8D 86", "C7 40 ?? ?? ?? ?? ?? 8B 45 ?? 8B 54 24",
		"C7 43 ?? ?? ?? ?? ?? D8 0D", "C7 42 ?? ?? ?? ?? ?? 8B C8", "6C 16 AD 3F AB AA AA 3F 35 58 A8 3F");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR5] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR6] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR7] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR8] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR9] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR10] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio 11: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScansResult[AR11] + 4 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScansResult, AR1, AR4, 3, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AR5] + 7, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult, AR6, AR10, 3, fNewAspectRatio);

			Memory::Write(AspectRatioScansResult[AR11] + 4, fNewAspectRatio);
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