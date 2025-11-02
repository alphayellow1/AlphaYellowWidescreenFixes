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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "AirborneHeroDDayFrontline1944WidescreenFix";
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
double dNewHorizontalRes;
uint8_t* CameraFOVAddress;
uint8_t* ResolutionWidth5Address;
uint8_t* ResolutionHeight5Address;

// Game detection
enum class Game
{
	AHDDF1944,
	Unknown
};

enum ResolutionListsScans
{
	Resolution1Scan,
	Resolution2Scan,
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AHDDF1944, {"Airborne Hero D - Day Frontline 1944", "AB_main.exe"}},
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

static SafetyHookMid ResolutionWidthInstruction1Hook{};
static SafetyHookMid ResolutionHeightInstruction1Hook{};
static SafetyHookMid ResolutionInstructions2Hook{};
static SafetyHookMid ResolutionInstructions3Hook{};
static SafetyHookMid ResolutionWidthInstruction4Hook{};
static SafetyHookMid ResolutionHeightInstruction4Hook{};
static SafetyHookMid ResolutionWidthInstruction5Hook{};
static SafetyHookMid ResolutionHeightInstruction5Hook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::AHDDF1944 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 74 24 14 A1 78 44 54 00 3B F0 7E 02 8B F0 8B 7C 24 18", "89 35 A4 44 54 00 89 35 A4 47 55 00 89 3D A8 44 54 00", "8B 7C 24 ?? 8B 5C 24 ?? 51", "8B 44 24 08 83 EC 10 85 C0 74 05 A3 F8 11 5E 00 8B 44 24 1C", "A3 ?? ?? ?? ?? FF D6 8B 74 24 ?? 48 A3 ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution5Scan] - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan], "\x90\x90\x90\x90", 4);

			ResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan] + 15, "\x90\x90\x90\x90", 4);

			ResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan] + 15, [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan], "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			ResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution3Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan], "\x90\x90\x90\x90", 4);

			ResolutionWidthInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan] + 16, "\x90\x90\x90\x90", 4);

			ResolutionHeightInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan] + 16, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ResolutionWidth5Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScansResult[Resolution5Scan] + 1, Memory::PointerMode::Absolute);

			ResolutionHeight5Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScansResult[Resolution5Scan] + 13, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan], "\x90\x90\x90\x90\x90", 5);

			ResolutionWidthInstruction5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth5Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan] + 12, "\x90\x90\x90\x90\x90", 5);

			ResolutionHeightInstruction5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan] + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight5Address) = iCurrentResY;
			});
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "DC 0D ?? ?? ?? ?? DB 05");
		if (AspectRatioInstructionScanResult)
		{			
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			dNewHorizontalRes = 240 * (double)fNewAspectRatio;

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(dNewHorizontalRes);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction scan memory address.");
			return;
		}
		
		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 E1 D9 15");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

				fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale);

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction scan memory address.");
			return;
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
