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
std::string sFixName = "TheWatchmakerWidescreenFix";
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
uint8_t* ResolutionWidth1Address;
uint8_t* ResolutionHeight1Address;
uint8_t* ResolutionWidth5Address;
uint8_t* ResolutionHeight5Address;
uint8_t* ResolutionWidth6Address;
uint8_t* ResolutionHeight6Address;
uint8_t* ResolutionWidth7Address;
uint8_t* ResolutionHeight7Address;

// Game detection
enum class Game
{
	TWM,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Resolution1Scan,
	Resolution2Scan,
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan,
	Resolution6Scan,
	Resolution7Scan,
	Resolution8Scan,
	Resolution9Scan,
	Resolution10Scan,
	Resolution11Scan,
	Resolution12Scan,
	Resolution13Scan,
	Resolution14Scan,
	Resolution15Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TWM, {"The Watchmaker", "Wm.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("render.dll")) == nullptr)
	{
		spdlog::warn("render.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for render.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x1C);

	fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::TWM && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "89 0D 18 B2 62 00 89 15 1C B2 62 00 BF 01 00 00 00 BE C4 F4 55 00", "8B 0D 40 55 AE 00 8B 15 90 55 AE 00 56 57", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 68",
			                                                                               dllModule2, "A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 89 35", "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 5E", "89 35 ?? ?? ?? ?? 89 35 ?? ?? ?? ?? 89 35 ?? ?? ?? ?? 89 35", "A3 ?? ?? ?? ?? 8B 44 24 ?? 6A 00 6A 00 68 ?? ?? ?? ?? 50 89 7C 24 ?? FF D6 85 C0 0F 85 ?? ?? ?? ?? 8B 44 24 ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 4C 24 ?? 8D 54 24 ?? 8D 44 24 ?? 52 50 89 0D ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is render.dll+{:x}", ResolutionInstructionsScansResult[Resolution4Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Resolution Instructions 5 Scan: Address is render.dll+{:x}", ResolutionInstructionsScansResult[Resolution5Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Resolution Instructions 6 Scan: Address is render.dll+{:x}", ResolutionInstructionsScansResult[Resolution6Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Resolution Instructions 7 Scan: Address is render.dll+{:x}", ResolutionInstructionsScansResult[Resolution7Scan] - (std::uint8_t*)dllModule2);

			ResolutionWidth1Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution1Scan] + 2, Memory::PointerMode::Absolute);

			ResolutionHeight1Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution1Scan] + 8, Memory::PointerMode::Absolute);

			ResolutionWidth5Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution5Scan] + 8, Memory::PointerMode::Absolute);

			ResolutionHeight5Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution5Scan] + 2, Memory::PointerMode::Absolute);

			ResolutionWidth6Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution6Scan] + 8, Memory::PointerMode::Absolute);

			ResolutionHeight6Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution6Scan] + 2, Memory::PointerMode::Absolute);

			ResolutionWidth7Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution7Scan] + 1, Memory::PointerMode::Absolute);

			ResolutionHeight7Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScansResult[Resolution7Scan] + 61, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions1MidHook{};

			ResolutionInstructions1MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;
			});

			static SafetyHookMid ResolutionInstructions2MidHook{};

			ResolutionInstructions2MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(0x00AE5540) = iCurrentResX;

				*reinterpret_cast<int*>(0x00AE5590) = iCurrentResY;
			});

			/*
			Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan] + 1, iCurrentResY);
			*/

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid ResolutionInstructions4MidHook{};

			ResolutionInstructions4MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions5MidHook{};

			ResolutionInstructions5MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth5Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight5Address) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution6Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions6MidHook{};

			ResolutionInstructions6MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution6Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth6Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight6Address) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution7Scan], "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction7MidHook{};

			ResolutionWidthInstruction7MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution7Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth7Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution7Scan] + 59, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction7MidHook{};

			ResolutionHeightInstruction7MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution7Scan] + 59, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight7Address) = iCurrentResY;
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 44 24 ?? D8 0D");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is render.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90", 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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