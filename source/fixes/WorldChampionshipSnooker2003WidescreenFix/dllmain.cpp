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
std::string sFixName = "WorldChampionshipSnooker2003WidescreenFix";
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
float fNewResX;
float fNewResY;
uint8_t* ResolutionWidth1Address;
uint8_t* ResolutionHeight1Address;
uint8_t* ResolutionWidth2Address;
uint8_t* ResolutionHeight2Address;
uint8_t* ResolutionWidth3Address;
uint8_t* ResolutionHeight3Address;
uint8_t* ResolutionWidth4Address;
uint8_t* ResolutionHeight4Address;
uint8_t* ResolutionWidth5Address;
uint8_t* ResolutionHeight5Address;
uint8_t* ResolutionWidth6Address;
uint8_t* ResolutionHeight6Address;
uint8_t* ResolutionWidth7Address;
uint8_t* ResolutionHeight7Address;
uint8_t* ResolutionWidth8Address;
uint8_t* ResolutionHeight8Address;

// Game detection
enum class Game
{
	WCS2003,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WCS2003, {"World Championship Snooker 2003", "wcs2003.exe"}},
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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewResY]
		fdiv dword ptr ds:[fNewResX]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::WCS2003 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		Sleep(1500);

		std::uint8_t* ResolutionSelectorInstructionsScanResult = Memory::PatternScan(exeModule, "8B 88 D8 C1 75 00 89 0A 8B 88 DC C1 75 00 8B 54 24 08 89 0A 8B 88 E4 C1 75 00 8B 54 24 0C 89 0A 8B 80 E8 C1 75 00");
		if (ResolutionSelectorInstructionsScanResult)
		{
			spdlog::info("Resolution Selector Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionSelectorInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionSelectorInstructionsScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionSelectorWidthInstructionMidHook;

			ResolutionSelectorWidthInstructionMidHook = safetyhook::create_mid(ResolutionSelectorInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionSelectorInstructionsScanResult + 8, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionSelectorHeightInstructionMidHook;

			ResolutionSelectorHeightInstructionMidHook = safetyhook::create_mid(ResolutionSelectorInstructionsScanResult + 8, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution selector instructions scan memory address.");
			return;
		}
		/*
		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? B8 01 00 00 00 C3 CC CC CC CC CC CC CC CC CC CC CC CC CC CC 89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ??");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth1Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight1Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 8, Memory::PointerMode::Absolute);

			ResolutionWidth2Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 34, Memory::PointerMode::Absolute);

			ResolutionHeight2Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 40, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructionsScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions1MidHook{};

			ResolutionInstructions1MidHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScanResult + 32, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions2MidHook{};

			ResolutionInstructions2MidHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 32, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth2Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight2Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}
		*/

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "89 15 ?? ?? ?? ?? 8B 54 24 2C 89 0D ?? ?? ?? ?? 8B 4C 24 54 5D A3 ?? ?? ?? ??");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 22, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth3Address) = iCurrentResX;				
			});

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 21, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 21, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight3Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 81 EC D4 00 00 00 69 C0 18 05 00 00 56 57 8D 74 24 08 56 8B B0 ?? ?? ?? ?? 8B 34 B5 ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? 56 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 61, Memory::PointerMode::Absolute);

			ResolutionHeight4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 1, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions3ScanResult + 59, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction3MidHook{};

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult + 59, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth4Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction3MidHook{};

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight4Address) = iCurrentResY;
			});			
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 8B 0F 89 0D ?? ?? ?? ?? 8B 57 14 89 15 ?? ?? ?? ?? 8B 47 0C A3 ?? ?? ?? ?? 8B 4F 04 89 0D ?? ?? ?? ?? 8B 17 89 15 ?? ?? ?? ??");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth5Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 9, Memory::PointerMode::Absolute);

			ResolutionHeight5Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionWidth6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 43, Memory::PointerMode::Absolute);

			ResolutionHeight6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 35, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 7, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction4MidHook{};

			ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 7, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth5Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction4MidHook{};

			ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight5Address) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 41, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction5MidHook{};

			ResolutionWidthInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 41, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth6Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 33, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction5MidHook{};

			ResolutionHeightInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 33, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight6Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 4 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? A3 ?? ?? ?? ?? B9 10 00 00 00");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions5ScanResult + 7, Memory::PointerMode::Absolute);

			ResolutionHeight7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions5ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions5ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid ResolutionInstructions5MidHook{};

			ResolutionInstructions5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth7Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight7Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 8B 0E 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions6ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 10, Memory::PointerMode::Absolute);

			ResolutionHeight8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 8, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction6MidHook{};

			ResolutionWidthInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult + 8, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth8Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions6ScanResult, "\x90\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction6MidHook{};

			ResolutionHeightInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight8Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 6 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 83 D0 00 00 00 D9 83 D0 00 00 00 51");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV = std::bit_cast<float>(ctx.eax);

				fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

				*reinterpret_cast<float*>(ctx.ebx + 0xD0) = fNewCameraFOV;
			});
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
