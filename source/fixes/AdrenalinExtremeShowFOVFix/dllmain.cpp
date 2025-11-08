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
HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "AdrenalinExtremeShowFOVFix";
std::string sFixVersion = "1.3";
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
float fNewCameraHFOV;
float fNewCameraFOV;
uint8_t* CameraFOVValue1Address;
uint8_t* CameraFOVValue2Address;
uint8_t* CameraFOVValue3Address;

// Game detection
enum class Game
{
	AES,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AES, {"Adrenalin: Extreme Show", "Adrenalin.exe"}},
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
	GetModuleFileNameW(dllModule, exePathW, MAX_PATH);
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

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};

void CameraFOVInstructionMidHook(uint8_t* CameraFOVAddress)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

	if (fCurrentCameraFOV != 1.732050776f && fCurrentCameraFOV != 3.23781991f && fCurrentCameraFOV != 2.144506931f && fCurrentCameraFOV != 1.428148031f)
	{
		fNewCameraFOV = fCurrentCameraFOV / fFOVFactor;
	}
	else
	{
		fNewCameraFOV = fCurrentCameraFOV;
	}

	FPU::FLD(fNewCameraFOV);
}

void FOVFix()
{
	if (eGameType == Game::AES && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		fNewCameraHFOV = fAspectRatioScale;

		std::uint8_t* CameraHFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "8B 2D ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 F9 D9 1C 24");
		if (CameraHFOVInstruction1ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction1ScanResult + 6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction1ScanResult + 6, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraHFOVInstruction1ScanResult + 6, [](SafetyHookContext& ctx) { FPU::FLD(fNewCameraHFOV); });
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 1 scan memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D8 4D 74 D9 05 ?? ?? ?? ?? D8 F9 D9 1C 24");
		if (CameraHFOVInstruction2ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction2ScanResult + 3 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction2ScanResult + 3, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstruction2ScanResult + 3, [](SafetyHookContext& ctx) { FPU::FLD(fNewCameraHFOV); });
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 2 scan memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D8 05 ?? ?? ?? ?? DB 44 24 30 DE C9 D9 5C 24 24 D9 44 24 24 DB 5C 24 0C 8B 44 24 0C DB 44 24 34");
		if (CameraHFOVInstruction3ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstruction3ScanResult, [](SafetyHookContext& ctx) { FPU::FADD(fNewCameraHFOV); });
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 3 scan memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "8B 1D ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 F9 D9 1C 24");
		if (CameraHFOVInstruction4ScanResult)
		{
			spdlog::info("Camera HFOV Instruction 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstruction4ScanResult + 6 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstruction4ScanResult + 6, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraHFOVInstruction4ScanResult + 6, [](SafetyHookContext& ctx) { FPU::FLD(fNewCameraHFOV); });
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction 4 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 8B 11 83 C4 F0 D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult + 11 - (std::uint8_t*)exeModule);

			CameraFOVValue1Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction1ScanResult + 13, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult + 11, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult + 11, [](SafetyHookContext& ctx) { CameraFOVInstructionMidHook(CameraFOVValue1Address); });
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "FF 92 FC 00 00 00 8B 0D ?? ?? ?? ?? 8B 31 83 C4 F0 D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult + 17 - (std::uint8_t*)exeModule);

			CameraFOVValue2Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction2ScanResult + 19, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult + 17, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 17, [](SafetyHookContext& ctx) { CameraFOVInstructionMidHook(CameraFOVValue2Address); });
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "50 6A 02 FF 92 FC 00 00 00 8B 43 04 8B 0D ?? ?? ?? ?? 8B 29 D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult + 20 - (std::uint8_t*)exeModule);

			CameraFOVValue3Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction3ScanResult + 22, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult + 20, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult + 20, [](SafetyHookContext& ctx) { CameraFOVInstructionMidHook(CameraFOVValue3Address); });
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 3 scan memory address.");
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
