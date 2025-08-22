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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "RedlineWidescreenFix";
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
int16_t iCurrentResX;
int16_t iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fNewCameraFOV;
float fAspectRatioScale;
int iDesiredBitDepth;
static uint8_t* BitDepthAddress;
static uint8_t* ResolutionWidthAddress;
static uint8_t* ResolutionHeightAddress;
static uint8_t* CameraFOV2Address;

// Game detection
enum class Game
{
	RL,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RL, {"Redline", "Redline.exe"}},
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

void WidescreenFix()
{
	if (eGameType == Game::RL && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionErrorJumpScanResult = Memory::PatternScan(exeModule, "75 13 68 58 E4 58 00 B9 70 3F 5C 00 E8 F7 2C 05 00");
		if (ResolutionErrorJumpScanResult)
		{
			spdlog::info("Resolution Error Jump Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionErrorJumpScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionErrorJumpScanResult, "\xEB", 1); // This makes the game jump the resolution error code and accept the custom resolution
		}
		else
		{
			spdlog::error("Failed to locate resolution error jump scan memory address.");
			return;
		}

		std::uint8_t* BithDepthInstructionScanResult = Memory::PatternScan(exeModule, "66 8B 0D ?? ?? ?? ?? 51 33 D2");
		if (BithDepthInstructionScanResult)
		{
			spdlog::info("Bit Depth Instruction: Address is {:s}+{:x}", sExeName.c_str(), BithDepthInstructionScanResult - (std::uint8_t*)exeModule);

			BitDepthAddress = Memory::GetPointer<uint32_t>(BithDepthInstructionScanResult + 3, Memory::PointerMode::Absolute);

			iDesiredBitDepth = 32;
			
			static SafetyHookMid BitDepthInstructionMidHook{};

			BitDepthInstructionMidHook = safetyhook::create_mid(BithDepthInstructionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(BitDepthAddress) = iDesiredBitDepth;
			});
		}
		else
		{
			spdlog::error("Failed to locate bit depth instruction memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "66 8B 15 ?? ?? ?? ?? 52 33 C0 66 A1 ?? ?? ?? ?? 50 33 C9");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			ResolutionWidthAddress = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 12, Memory::PointerMode::Absolute);

			ResolutionHeightAddress = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 3, Memory::PointerMode::Absolute);

			static SafetyHookMid ResolutionWidthInstructionMidHook{};

			ResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 10, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int16_t*>(ResolutionWidthAddress) = iCurrentResX;
			});

			static SafetyHookMid ResolutionHeightInstructionMidHook{};

			ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int16_t*>(ResolutionHeightAddress) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "C7 81 C8 07 00 00 00 00 80 3F 8B 55 FC C7 82 B4 07 00 00 00 00 80 3F");
		if (CameraFOVInstructionsScanResult)
		{
			spdlog::info("Camera FOV Instructions: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV = 1.0f / fAspectRatioScale;

			Memory::Write(CameraFOVInstructionsScanResult + 6, fNewCameraFOV);

			Memory::Write(CameraFOVInstructionsScanResult + 19, fNewCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
		
		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 15 ?? ?? ?? ?? 52 D9 45 CC D8 65 9C 51 D9 1C 24");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			CameraFOV2Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction2ScanResult, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction2MidHook{};
			
			CameraFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOV2Address);

				fNewCameraFOV = fCurrentCameraFOV * (1.0f / fFOVFactor);

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
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