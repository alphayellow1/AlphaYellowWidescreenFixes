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
std::string sFixName = "Frogger2SwampysRevengeWidescreenFix";
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
int iNewCameraFOV1;
int iNewCameraFOV2;
int iNewCameraFOV3;
int iNewCameraFOV4;
int iNewCameraFOV5;
int iNewCameraFOV6;
int iNewCameraFOV7;
int iNewCameraFOV8;
uint8_t* CameraFOV1Address;
uint8_t* CameraFOV2Address;
uint8_t* CameraFOV3Address;
uint8_t* CameraFOV4Address;
uint8_t* CameraFOV5Address;
uint8_t* CameraFOV6Address;
uint8_t* CameraFOV7Address;
uint8_t* CameraFOV8Address;

// Game detection
enum class Game
{
	F2SR,
	Unknown
};

enum ResolutionInstructionsIndex
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan,
	CameraFOV4Scan,
	CameraFOV5Scan,
	CameraFOV6Scan,
	CameraFOV7Scan,
	CameraFOV8Scan,
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::F2SR, {"Frogger 2: Swampy's Revenge", "Frogger2(1).exe"}},
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

static SafetyHookMid CameraFOVInstruction7Hook{};

void CameraFOVInstruction7MidHook(SafetyHookContext& ctx)
{
	int& iCurrentCameraFOV7 = *reinterpret_cast<int*>(CameraFOV7Address);

	iNewCameraFOV7 = (int)((iCurrentCameraFOV7 / fAspectRatioScale) / fFOVFactor);

	_asm
	{
		fimul dword ptr ds:[iNewCameraFOV7]
	}
}

static SafetyHookMid CameraFOVInstruction8Hook{};

void CameraFOVInstruction8MidHook(SafetyHookContext& ctx)
{
	int& iCurrentCameraFOV8 = *reinterpret_cast<int*>(CameraFOV8Address);

	iNewCameraFOV8 = (int)((iCurrentCameraFOV8 / fAspectRatioScale) / fFOVFactor);

	_asm
	{
		fild dword ptr ds:[iNewCameraFOV8]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::F2SR && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "C7 44 24 34 40 01 00 00 C7 44 24 38 F0 00 00 00 C7 44 24 3C 80 02 00 00 C7 44 24 40 E0 01 00 00 C7 44 24 44 20 03 00 00 C7 44 24 48 58 02 00 00 C7 44 24 4C 00 04 00 00 C7 44 24 50 00 03 00 00");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScanResult + 52, iCurrentResX);

			Memory::Write(ResolutionInstructionsScanResult + 60, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 03 C3", "8B 0D ?? ?? ?? ?? F7 D9 89 4C 24 ?? DB 44 24 ?? D8 0C 85 ?? ?? ?? ?? D9 C0 D8 0F", "A1 ?? ?? ?? ?? 89 4C 24 ?? 8B 0D ?? ?? ?? ?? 89 54 24 ?? 8B 15 ?? ?? ?? ?? 89 4C 24", "8B 1D ?? ?? ?? ?? 55 56 57 8B 3D", "8B 0D ?? ?? ?? ?? F7 D9 89 4C 24 ?? DB 44 24 ?? D8 0C 85 ?? ?? ?? ?? D9 C0 D8 4A", "8B 0D ?? ?? ?? ?? F7 D9 89 4C 24 ?? DB 44 24 ?? D8 0C 85 ?? ?? ?? ?? D9 C0 D8 8A ?? ?? ?? ?? D8 05 ?? ?? ?? ?? D9 9A ?? ?? ?? ?? D8 8A", "DA 0D ?? ?? ?? ?? D8 4C 24", "DB 05 ?? ?? ?? ?? DB 05");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV7Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV8Scan] - (std::uint8_t*)exeModule);

			CameraFOV1Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV1Scan] + 2, Memory::PointerMode::Absolute);

			CameraFOV2Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV2Scan] + 2, Memory::PointerMode::Absolute);

			CameraFOV3Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV3Scan] + 1, Memory::PointerMode::Absolute);

			CameraFOV4Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV4Scan] + 2, Memory::PointerMode::Absolute);

			CameraFOV5Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV5Scan] + 2, Memory::PointerMode::Absolute);

			CameraFOV6Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV6Scan] + 2, Memory::PointerMode::Absolute);

			CameraFOV7Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV7Scan] + 2, Memory::PointerMode::Absolute);

			CameraFOV8Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionsScansResult[CameraFOV8Scan] + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction1MidHook{};

			CameraFOVInstruction1MidHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV1 = *reinterpret_cast<int*>(CameraFOV1Address);

				iNewCameraFOV1 = (int)((iCurrentCameraFOV1 / fAspectRatioScale) / fFOVFactor);

				ctx.ecx = std::bit_cast<uintptr_t>(iNewCameraFOV1);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction2MidHook{};

			CameraFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV2 = *reinterpret_cast<int*>(CameraFOV2Address);

				iNewCameraFOV2 = (int)((iCurrentCameraFOV2 / fAspectRatioScale) / fFOVFactor);

				ctx.ecx = std::bit_cast<uintptr_t>(iNewCameraFOV2);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV3Scan], "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid CameraFOVInstruction3MidHook{};

			CameraFOVInstruction3MidHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV3Scan], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV3 = *reinterpret_cast<int*>(CameraFOV3Address);

				iNewCameraFOV3 = (int)((iCurrentCameraFOV3 / fAspectRatioScale) / fFOVFactor);

				ctx.eax = std::bit_cast<uintptr_t>(iNewCameraFOV3);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction4MidHook{};

			CameraFOVInstruction4MidHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV4Scan], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV4 = *reinterpret_cast<int*>(CameraFOV4Address);

				iNewCameraFOV4 = (int)((iCurrentCameraFOV4 / fAspectRatioScale) / fFOVFactor);

				ctx.ebx = std::bit_cast<uintptr_t>(iNewCameraFOV4);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction5MidHook{};

			CameraFOVInstruction5MidHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV5Scan], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV5 = *reinterpret_cast<int*>(CameraFOV5Address);

				iNewCameraFOV5 = (int)((iCurrentCameraFOV5 / fAspectRatioScale) / fFOVFactor);

				ctx.ecx = std::bit_cast<uintptr_t>(iNewCameraFOV5);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction6MidHook{};

			CameraFOVInstruction6MidHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV6Scan], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV6 = *reinterpret_cast<int*>(CameraFOV6Address);

				iNewCameraFOV6 = (int)((iCurrentCameraFOV6 / fAspectRatioScale) / fFOVFactor);

				ctx.ecx = std::bit_cast<uintptr_t>(iNewCameraFOV6);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV7Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV7Scan], CameraFOVInstruction7MidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV8Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction8Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV8Scan], CameraFOVInstruction8MidHook);
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
