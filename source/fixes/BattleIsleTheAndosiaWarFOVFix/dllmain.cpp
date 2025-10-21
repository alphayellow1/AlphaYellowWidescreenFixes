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

// Fix details
std::string sFixName = "BattleIsleTheAndosiaWarFOVFix";
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
float fAspectRatioScale;
float fNewAspectRatio;
float fCurrentMenuCameraFOVFactor;
float fCurrentMenuCameraFOV;
float fNewCameraFOV;
float fNewMenuCameraFOV;
float fNewMenuCameraFOVFactor;
uint8_t* CameraFOVAddress;
uint8_t* MenuCameraFOVAddress;

// Game detection
enum class Game
{
	BITAW,
	Unknown
};

enum MenuCameraFOVInstructionsIndex
{
	MenuFOV1Scan,
	MenuFOV2Scan,
	MenuFOV3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BITAW, {"Battle Isle: The Andosia War", "bitaw.exe"}},
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

static SafetyHookMid CameraFOVInstructionHook{};

static SafetyHookMid MenuCameraFOVInstruction1Hook{};
static SafetyHookMid MenuCameraFOVInstruction2Hook{};
static SafetyHookMid MenuCameraFOVInstruction3Hook{};

void FOVFix()
{
	if (eGameType == Game::BITAW && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 89 1D ?? ?? ?? ?? C6 05");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 1, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90", 5); // NOP out the original instruction
			
			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV = std::bit_cast<float>(ctx.eax);

				fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;				

				*reinterpret_cast<float*>(CameraFOVAddress) = fNewCameraFOV;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> MenuCameraFOVInstructionsScansRsesult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 3B FE", "D8 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 3B F3 0F 84 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? B9 ?? ?? ?? ?? 3B C1 74 ?? A1 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 51 6A ?? 8B 10 50 FF 52 ?? B9 ?? ?? ?? ?? 39 0D ?? ?? ?? ?? 74 ?? A1 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 51 6A ?? 8B 10 50 FF 52 ?? B9 ?? ?? ?? ?? 39 0D ?? ?? ?? ?? 74 ?? A1 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 51 6A ?? 8B 10 50 FF 52 ?? 53 57 8B CE E8 ?? ?? ?? ?? 50 56 E8 ?? ?? ?? ?? 83 C4 ?? 8B 35 ?? ?? ?? ?? 6A ?? 33 FF E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 3B F3 0F 84 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? 89 1D ?? ?? ?? ?? EB ?? D9 05 ?? ?? ?? ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? 89 2D ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 48 89 5C 24 ?? 89 44 24 ?? DF 6C 24 ?? D9 5C 24 ?? D9 44 24 ?? D8 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F8 53 57 68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 50 55 53 53 53 8D 4C 24 ?? E8 ?? ?? ?? ?? 50 8D 44 24 ?? 50 8B CE E8 ?? ?? ?? ?? 53 57 68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 50 55 53 53 53 8D 4C 24 ?? E8 ?? ?? ?? ?? 8D 4C 24 ?? 50 51 8B CE E8 ?? ?? ?? ?? 53 57 68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 50 55 53 53 53 8D 4C 24 ?? E8 ?? ?? ?? ?? 8D 54 24 ?? 50 52 8B CE E8 ?? ?? ?? ?? 8D 44 24 ?? 50 E8 ?? ?? ?? ?? 51 DD 1C ?? E8 ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 74 24 ?? 83 C4 ?? D9 1C ?? D9 44 24 ?? D8 74 24 ?? 51 D9 1C ?? D9 44 24 ?? D8 74 24 ?? 51 D9 1C ?? 8D 4C 24 ?? E8 ?? ?? ?? ?? 8B 4C 24 ?? 8B 54 24 ?? 8B 44 24 ?? 6A ?? 89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 4C 24 ?? 53 8D 54 24 ?? 51 8D 44 24 ?? 52 50 89 5C 24 ?? 89 6C 24 ?? 89 5C 24 ?? E8 ?? ?? ?? ?? 83 C4 ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 3B F3 0F 84 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? BD ?? ?? ?? ?? 3B C5 74 ?? A1 ?? ?? ?? ?? 89 2D ?? ?? ?? ?? 55 6A ?? 8B 08 50 FF 51 ?? 39 2D ?? ?? ?? ?? 74 ?? A1 ?? ?? ?? ?? 89 2D ?? ?? ?? ?? 55 6A ?? 8B 10 50 FF 52 ?? 39 2D ?? ?? ?? ?? 74 ?? A1 ?? ?? ?? ?? 89 2D ?? ?? ?? ?? 55 6A ?? 8B 08 50 FF 51 ?? 53 57 8B CE E8 ?? ?? ?? ?? 50 56 E8 ?? ?? ?? ?? 83 C4 ?? EB ?? BD ?? ?? ?? ?? 53 E8 ?? ?? ?? ?? 68", "D8 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 3B F3 0F 84 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? BD ?? ?? ?? ?? 3B C5 74 ?? A1 ?? ?? ?? ?? 89 2D ?? ?? ?? ?? 55 6A ?? 8B 08 50 FF 51 ?? 39 2D ?? ?? ?? ?? 74 ?? A1 ?? ?? ?? ?? 89 2D ?? ?? ?? ?? 55 6A ?? 8B 10 50 FF 52 ?? 39 2D ?? ?? ?? ?? 74 ?? A1 ?? ?? ?? ?? 89 2D ?? ?? ?? ?? 55 6A ?? 8B 08 50 FF 51 ?? 53 57 8B CE E8 ?? ?? ?? ?? 50 56 E8 ?? ?? ?? ?? 83 C4 ?? EB ?? BD ?? ?? ?? ?? 53 E8 ?? ?? ?? ?? 68");
		if (Memory::AreAllSignaturesValid(MenuCameraFOVInstructionsScansRsesult) == true)
		{
			spdlog::info("Menu Camera FOV Instruction 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuCameraFOVInstructionsScansRsesult[MenuFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Camera FOV Instruction 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuCameraFOVInstructionsScansRsesult[MenuFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Camera FOV Instruction 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), MenuCameraFOVInstructionsScansRsesult[MenuFOV3Scan] - (std::uint8_t*)exeModule);

			MenuCameraFOVAddress = Memory::GetPointer<uint32_t>(MenuCameraFOVInstructionsScansRsesult[MenuFOV1Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(MenuCameraFOVInstructionsScansRsesult[MenuFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			MenuCameraFOVInstruction1Hook = safetyhook::create_mid(MenuCameraFOVInstructionsScansRsesult[MenuFOV1Scan], [](SafetyHookContext& ctx)
			{
				fCurrentMenuCameraFOVFactor = *reinterpret_cast<float*>(MenuCameraFOVAddress);

				fCurrentMenuCameraFOV = Maths::Pi<float> * fCurrentMenuCameraFOVFactor;

				fNewMenuCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentMenuCameraFOV, fAspectRatioScale);

				fNewMenuCameraFOVFactor = fNewMenuCameraFOV / Maths::Pi<float>;

				FPU::FMUL(fNewMenuCameraFOVFactor);
			});

			Memory::PatchBytes(MenuCameraFOVInstructionsScansRsesult[MenuFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			MenuCameraFOVInstruction2Hook = safetyhook::create_mid(MenuCameraFOVInstructionsScansRsesult[MenuFOV2Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewMenuCameraFOVFactor);
			});

			Memory::PatchBytes(MenuCameraFOVInstructionsScansRsesult[MenuFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			MenuCameraFOVInstruction3Hook = safetyhook::create_mid(MenuCameraFOVInstructionsScansRsesult[MenuFOV3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewMenuCameraFOVFactor);
			});
		}
		else
		{
			spdlog::error("Failed to locate menu camera FOV instruction memory address.");
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