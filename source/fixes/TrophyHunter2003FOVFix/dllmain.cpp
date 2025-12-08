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
std::string sFixName = "TrophyHunter2003FOVFix";
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
float fNewAspectRatio2;
float fNewCameraFOV;
float fNewHipfireFOV;
float fNewUnarmedHipfireFOV;
uint8_t* HipfireFOVAddress;

// Game detection
enum class Game
{
	TH2003,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan
};

enum CameraFOVInstructionsIndices
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan,
	SniperHipfireFOVScan,
	BinocularsHipfireFOVScan,
	UnarmedHipfireFOVScan,
	RifleHipfireFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TH2003, {"Trophy Hunter 2003", "TH2003.exe"}},
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
	bool bGameFound = false;

	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			bGameFound = true;
			break;
		}
	}

	if (bGameFound == false)
	{
		spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		return false;
	}

	dllModule2 = Memory::GetHandle("Aspen.dll");

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid SniperHipfireFOVInstructionHook{};
static SafetyHookMid BinocularsHipfireFOVInstructionHook{};
static SafetyHookMid UnarmedHipfireFOVInstructionHook{};
static SafetyHookMid RifleHipfireFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0x7C);

	fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale);

	FPU::FLD(fNewCameraFOV);
}

void HipfireFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentHipfireFOV = *reinterpret_cast<float*>(HipfireFOVAddress);

	fNewHipfireFOV = fCurrentHipfireFOV * fFOVFactor;

	FPU::FMUL(fNewHipfireFOV);
}

void FOVFix()
{
	if (eGameType == Game::TH2003 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "D9 40 ?? C9 C3 55 8B EC 83 EC ?? 89 4D ?? 8B 45 ?? 83 78");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is Aspen.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90", 3); // NOP out the original instruction

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.eax + 0x60);

				if (fCurrentAspectRatio == 1.330000043f)
				{
					fNewAspectRatio2 = Maths::CalculateNewFOV_RadBased(fCurrentAspectRatio, fAspectRatioScale);
				}
				else
				{
					fNewAspectRatio2 = fCurrentAspectRatio;
				}

				FPU::FLD(fNewAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 40 ?? DC 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 7B ?? 8B 45 ?? D9 40 ?? DC 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 7A", "D9 40 ?? D8 0D ?? ?? ?? ?? D9 55", "D9 40 ?? C9 C3 55 8B EC 51 89 4D ?? 8B 45 ?? 8B 4D", exeModule, "D8 0D ?? ?? ?? ?? D9 45 ?? D8 4D", "D8 0D ?? ?? ?? ?? 8B 45 ?? D9 45", "D9 05 ?? ?? ?? ?? D9 98 ?? ?? ?? ?? EB ?? 8B 4D", "D8 0D ?? ?? ?? ?? 8B 45 ?? 8B 40 ?? D9 45");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is Aspen.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 2: Address is Aspen.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 3: Address is Aspen.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Sniper Hipfire FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[SniperHipfireFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Binoculars Hipfire FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[BinocularsHipfireFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Unarmed Hipfire FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[UnarmedHipfireFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Rifle Hipfire FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[RifleHipfireFOVScan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], CameraFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], CameraFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV3Scan], "\x90\x90\x90", 3); // NOP out the original instruction

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV3Scan], CameraFOVInstructionMidHook);

			// Hipfire FOVs
			HipfireFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[SniperHipfireFOVScan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[SniperHipfireFOVScan], "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			SniperHipfireFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[SniperHipfireFOVScan], HipfireFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[BinocularsHipfireFOVScan], "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			BinocularsHipfireFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[BinocularsHipfireFOVScan], HipfireFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[UnarmedHipfireFOVScan], "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			UnarmedHipfireFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[UnarmedHipfireFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentUnarmedHipfireFOV = *reinterpret_cast<float*>(HipfireFOVAddress);

				fNewUnarmedHipfireFOV = fCurrentUnarmedHipfireFOV * fFOVFactor;

				FPU::FLD(fNewUnarmedHipfireFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[RifleHipfireFOVScan], "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			RifleHipfireFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[RifleHipfireFOVScan], HipfireFOVInstructionMidHook);
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