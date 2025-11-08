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
#include <algorithm>
#include <cmath>
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "UltimateDemolitionDerbyWidescreenFix";
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

// Ini variables
bool bFixActive;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fCurrentCameraHFOV;
float fCurrentCameraVFOV;
float fFOVFactor;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;
uint8_t* ResolutionWidthAddress;
uint8_t* ResolutionHeightAddress;
uint8_t* CameraVFOVAddress;

// Game detection
enum class Game
{
	UDD,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::UDD, {"Ultimate Demolition Derby", "UDD.exe"}},
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

static SafetyHookMid ViewportResolutionWidthInstruction1Hook{};

void ViewportResolutionWidthInstruction1MidHook(SafetyHookContext& ctx)
{	
	_asm
	{
		fild dword ptr ds:[iCurrentResX]
	}
}

static SafetyHookMid ViewportResolutionHeightInstruction1Hook{};

void ViewportResolutionHeightInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResY]
	}
}

static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};

void ViewportResolutionWidthInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fidiv dword ptr ds:[iCurrentResX]
	}
}

static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};

void ViewportResolutionHeightInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResY]
	}
}

static SafetyHookMid ViewportResolutionWidthInstruction3Hook{};

void ViewportResolutionWidthInstruction3MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResX]
	}
}

static SafetyHookMid ViewportResolutionHeightInstruction3Hook{};

void ViewportResolutionHeightInstruction3MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResY]
	}
}

static SafetyHookMid ViewportResolutionHeightInstruction48Hook{};

void ViewportResolutionHeightInstruction48MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResY]
	}
}

static SafetyHookMid ViewportResolutionHeightInstruction49Hook{};

void ViewportResolutionHeightInstruction49MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResY]
	}
}

static SafetyHookMid CameraVFOVInstructionHook{};

void CameraVFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fCurrentCameraVFOV = *reinterpret_cast<float*>(CameraVFOVAddress);

	fNewCameraVFOV = (fCurrentCameraVFOV * fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fmul dword ptr ds:[fNewCameraVFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::UDD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "89 0D E0 F7 55 00 8B 57 04 89 15 E4 F7 55 00");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);
			
			ResolutionWidthAddress = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScanResult + 2, Memory::PointerMode::Absolute);;

			ResolutionHeightAddress = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScanResult + 11, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructionsScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ResolutionWidthInstructionMidHook{};
			
			ResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidthAddress) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScanResult + 9, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ResolutionHeightInstructionMidHook{};
			
			ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 9, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeightAddress) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "68 58 02 00 00 68 20 03 00 00 8B BC 24 5C 10 00 00 A3 CC CE 54 00");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(ResolutionInstructions2ScanResult + 6, iCurrentResX);

			Memory::Write(ResolutionInstructions2ScanResult + 1, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "DB 86 D4 00 00 00 D9 1C 24 DB 86 D0 00 00 00");
		if (ViewportResolutionInstructions1ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions1ScanResult + 9, "\x90\x90\x90\x90\x90\x90", 6);
			
			ViewportResolutionWidthInstruction1Hook = safetyhook::create_mid(ViewportResolutionInstructions1ScanResult + 9, ViewportResolutionWidthInstruction1MidHook);

			Memory::PatchBytes(ViewportResolutionInstructions1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);			

			ViewportResolutionHeightInstruction1Hook = safetyhook::create_mid(ViewportResolutionInstructions1ScanResult, ViewportResolutionHeightInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "DB 80 D4 00 00 00 56 57 C7 44 24 54 33 33 33 3F C7 44 24 4C CD CC 8C 3F DA B0 D0 00 00 00");
		if (ViewportResolutionInstructions2ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions2ScanResult + 24, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionWidthInstruction2Hook = safetyhook::create_mid(ViewportResolutionInstructions2ScanResult + 24, ViewportResolutionWidthInstruction2MidHook);

			Memory::PatchBytes(ViewportResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction2Hook = safetyhook::create_mid(ViewportResolutionInstructions2ScanResult, ViewportResolutionHeightInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "DB 81 D4 00 00 00 D9 1C 24 DB 81 D0 00 00 00");
		if (ViewportResolutionInstructions3ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions3ScanResult + 9, "\x90\x90\x90\x90\x90\x90", 6);
			
			ViewportResolutionWidthInstruction3Hook = safetyhook::create_mid(ViewportResolutionInstructions3ScanResult + 9, ViewportResolutionWidthInstruction3MidHook);
			
			Memory::PatchBytes(ViewportResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);			
			
			ViewportResolutionHeightInstruction3Hook = safetyhook::create_mid(ViewportResolutionInstructions3ScanResult, ViewportResolutionHeightInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction4ScanResult = Memory::PatternScan(exeModule, "8B 88 D0 00 00 00 57 8B 96 78 05 00 00");
		if (ViewportResolutionWidthInstruction4ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction4ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction4MidHook{};
			
			ViewportResolutionWidthInstruction4MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction4ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 4 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWdithInstruction5ScanResult = Memory::PatternScan(exeModule, "8B 88 D0 00 00 00 81 E9 82 00 00 00 51 52 8B CE E8 F6 93 FF FF EB 24");
		if (ViewportResolutionWdithInstruction5ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWdithInstruction5ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWdithInstruction5ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction5MidHook{};
			
			ViewportResolutionWidthInstruction5MidHook = safetyhook::create_mid(ViewportResolutionWdithInstruction5ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 5 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction6ScanResult = Memory::PatternScan(exeModule, "8B 88 D0 00 00 00 83 E9 78 51 52 E8 6C 8A 00 00 A1 20 CD 54 00");
		if (ViewportResolutionWidthInstruction6ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction6ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction6ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction6MidHook{};
			
			ViewportResolutionWidthInstruction6MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction6ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 6 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction7ScanResult = Memory::PatternScan(exeModule, "8B 88 D0 00 00 00 8B 84 96 4C 06 00 00 83 E9 32 68 96 00 00 00 51 8B 88 64 12 00 00");
		if (ViewportResolutionWidthInstruction7ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction7ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction7ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction7MidHook{};
			
			ViewportResolutionWidthInstruction7MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction7ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 7 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions8ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 8B 90 D0 00 00 00 8B 86 84 05 00 00 83 E9 78 83 EA 66");
		if (ViewportResolutionInstructions8ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions8ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions8ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionWidthInstruction8MidHook{};
			
			ViewportResolutionWidthInstruction8MidHook = safetyhook::create_mid(ViewportResolutionInstructions8ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 8 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions9ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 8B 90 D0 00 00 00 8B 86 70 06 00 00 83 E9 5A 51 81 EA 8C 00 00 00");
		if (ViewportResolutionInstructions9ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions9ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions9ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionWidthInstruction9MidHook{};
			
			ViewportResolutionWidthInstruction9MidHook = safetyhook::create_mid(ViewportResolutionInstructions9ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 9 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions10ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 8B 90 D0 00 00 00 8B 86 6C 05 00 00 83 E9 5A 83 EA 5A 51 52 50 E8 33 89 00 00");
		if (ViewportResolutionInstructions10ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions10ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions10ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionWidthInstruction10MidHook{};
			
			ViewportResolutionWidthInstruction10MidHook = safetyhook::create_mid(ViewportResolutionInstructions10ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 10 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions11ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 8B 90 D0 00 00 00 A1 B8 7B 52 00 83 E9 5A 83 EA 32 51 52 50 8B CE E8 8A 8F FF FF");
		if (ViewportResolutionInstructions11ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 11 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions11ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions11ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionWidthInstruction11MidHook{};
			
			ViewportResolutionWidthInstruction11MidHook = safetyhook::create_mid(ViewportResolutionInstructions11ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
				
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 11 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions12ScanResult = Memory::PatternScan(exeModule, "8B 90 D0 00 00 00 89 54 24 10 8B 80 D4 00 00 00 8D 54 24 10 89 44 24 14 52 E8 30 03 00 00");
		if (ViewportResolutionInstructions12ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 12 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions12ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions12ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction12MidHook{};

			ViewportResolutionWidthInstruction12MidHook = safetyhook::create_mid(ViewportResolutionInstructions12ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions12ScanResult + 10, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionHeightInstruction12MidHook{};
			
			ViewportResolutionHeightInstruction12MidHook = safetyhook::create_mid(ViewportResolutionInstructions12ScanResult + 10, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 12 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions13ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 89 10 8B 89 D4 00 00 00 89 48 04 C2 04 00");
		if (ViewportResolutionInstructions13ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 13 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions13ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions13ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction13MidHook{};
			
			ViewportResolutionWidthInstruction13MidHook = safetyhook::create_mid(ViewportResolutionInstructions13ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions13ScanResult + 8, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionHeightInstruction13MidHook{};
			
			ViewportResolutionHeightInstruction13MidHook = safetyhook::create_mid(ViewportResolutionInstructions13ScanResult + 8, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 13 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions14ScanResult = Memory::PatternScan(exeModule, "8B 82 D0 00 00 00 99 2B C2 D1 F8 2D 80 00 00 00 89 86 8C 04 00 00 A1 20 CD 54 00 8B 80 D4 00 00 00");
		if (ViewportResolutionInstructions14ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 14 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions14ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions14ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction14MidHook{};
			
			ViewportResolutionWidthInstruction14MidHook = safetyhook::create_mid(ViewportResolutionInstructions14ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});
			Memory::PatchBytes(ViewportResolutionInstructions14ScanResult + 27, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionHeightInstruction14MidHook{};
			
			ViewportResolutionHeightInstruction14MidHook = safetyhook::create_mid(ViewportResolutionInstructions14ScanResult + 27, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 14 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions15ScanResult = Memory::PatternScan(exeModule, "8B 90 D4 00 00 00 8B 80 D0 00 00 00 52 50 E8 07 16 00 00 85 C0 0F 84 78 01 00 00");
		if (ViewportResolutionInstructions15ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 15 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions15ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions15ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionInstructions15MidHook{};
			
			ViewportResolutionInstructions15MidHook = safetyhook::create_mid(ViewportResolutionInstructions15ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 15 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions16ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 8B 90 D0 00 00 00 8B 46 04 D1 F9 D1 FA 81 E9 2C 01 00 00 81 EA 90 01 00 00 51 52 50 E8 E2 ED FF FF");
		if (ViewportResolutionInstructions16ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 16 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions16ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions16ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionInstructions16MidHook{};
			
			ViewportResolutionInstructions16MidHook = safetyhook::create_mid(ViewportResolutionInstructions16ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 16 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction17ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 D1 FA 89 54 24 1C DB 44 24 1C D8 25 FC C5 51 00");
		if (ViewportResolutionWidthInstruction17ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 17 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction17ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction17ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction17MidHook{};
			
			ViewportResolutionWidthInstruction17MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction17ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 17 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction18ScanResult = Memory::PatternScan(exeModule, "8B 90 D0 00 00 00 6A 00 6A 01 6A 00 6A 00 8D 4E 1C D1 FA 6A 03");
		if (ViewportResolutionWidthInstruction18ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 18 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction18ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction18ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction18MidHook{};
			
			ViewportResolutionWidthInstruction18MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction18ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 18 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction19ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 01 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 6A 64 52 8B CF E8 DC D7 00 00");
		if (ViewportResolutionWidthInstruction19ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 19 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction19ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction19ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction19MidHook{};
			
			ViewportResolutionWidthInstruction19MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction19ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 19 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction20ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 02 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 87 00 00 00");
		if (ViewportResolutionWidthInstruction20ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 20 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction20ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction20ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction20MidHook{};
			
			ViewportResolutionWidthInstruction20MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction20ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 20 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction21ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 03 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 A0 00 00 00");
		if (ViewportResolutionWidthInstruction21ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 21 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction21ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction21ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction21MidHook{};
			
			ViewportResolutionWidthInstruction21MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction21ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 21 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction22ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 04 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 B9 00 00 00");
		if (ViewportResolutionWidthInstruction22ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 22 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction22ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction22ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction22MidHook{};
			
			ViewportResolutionWidthInstruction22MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction22ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 22 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction23ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 8B CF D1 FA 81 EA 77 01 00 00 52 E8 14 D7 00 00 8B 0D 20 CD 54 00");
		if (ViewportResolutionWidthInstruction23ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 23 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction23ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction23ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction23MidHook{};
			
			ViewportResolutionWidthInstruction23MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction23ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 23 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction24ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 06 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 1D 01 00 00");
		if (ViewportResolutionWidthInstruction24ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 24 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction24ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction24ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction24MidHook{};
			
			ViewportResolutionWidthInstruction24MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction24ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 24 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction25ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 07 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 36 01 00 00");
		if (ViewportResolutionWidthInstruction25ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 25 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction25ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction25ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction25MidHook{};
			
			ViewportResolutionWidthInstruction25MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction25ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 25 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction26ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 08 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 4F 01 00 00");
		if (ViewportResolutionWidthInstruction26ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 26 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction26ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction26ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction26MidHook{};
			
			ViewportResolutionWidthInstruction26MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction26ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 26 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction27ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 6A 00 8D 86 1C 09 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00");
		if (ViewportResolutionWidthInstruction27ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 27 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction27ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction27ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction27MidHook{};
			
			ViewportResolutionWidthInstruction27MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction27ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 27 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions28ScanResult = Memory::PatternScan(exeModule, "8B 80 D0 00 00 00 68 8C F5 52 00 99 2B C2 D1 F8 2D B4 00 00 00 89 83 8C 04 00 00 8B 15 20 CD 54 00 8B 82 D4 00 00 00");
		if (ViewportResolutionInstructions28ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 28 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions28ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions28ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction28MidHook{};
			
			ViewportResolutionWidthInstruction28MidHook = safetyhook::create_mid(ViewportResolutionInstructions28ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions28ScanResult + 33, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionHeightInstruction28MidHook{};
			
			ViewportResolutionHeightInstruction28MidHook = safetyhook::create_mid(ViewportResolutionInstructions28ScanResult + 33, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 28 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction29ScanResult = Memory::PatternScan(exeModule, "8B 88 D0 00 00 00 D1 F9 51 8B CE E8 0A 8F 00 00 8B 96 F0 04 00 00");
		if (ViewportResolutionWidthInstruction29ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 29 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction29ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction29ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction29MidHook{};
			
			ViewportResolutionWidthInstruction29MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction29ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 29 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions30ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 8B 90 D0 00 00 00 8B 46 04 D1 F9 D1 FA 81 E9 80 00 00 00 81 EA 80 00 00 00 51 8B 8E DC 05 00 00 52 50 57 E8 AF CF 05 00");
		if (ViewportResolutionInstructions30ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 30 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions30ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions30ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionInstructions30MidHook{};
			
			ViewportResolutionInstructions30MidHook = safetyhook::create_mid(ViewportResolutionInstructions30ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 30 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction31ScanResult = Memory::PatternScan(exeModule, "8B 82 D0 00 00 00 6A 05 6A 02 68 A8 87 52 00 D1 F8 6A 19 50 EB 40");
		if (ViewportResolutionWidthInstruction31ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 31 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction31ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction31ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction31MidHook{};
			
			ViewportResolutionWidthInstruction31MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction31ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 31 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction32ScanResult = Memory::PatternScan(exeModule, "8B 82 D0 00 00 00 6A 02 57 68 91 00 00 00 D1 F8 50 EB 3A 6A 05 83 FF 02 6A 04 6A 02 7E 19");
		if (ViewportResolutionWidthInstruction32ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 32 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction32ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction32ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction32MidHook{};
			
			ViewportResolutionWidthInstruction32MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction32ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 32 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction33ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 D1 FA 52 EB 16 6A 06 57 68 87 00 00 00 A1 20 CD 54 00");
		if (ViewportResolutionWidthInstruction33ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 33 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction33ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction33ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction33MidHook{};
			
			ViewportResolutionWidthInstruction33MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction33ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 33 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions34ScanResult = Memory::PatternScan(exeModule, "8B 80 D0 00 00 00 99 2B C2 D1 F8 2D 90 01 00 00 89 86 8C 04 00 00 8B 0D 20 CD 54 00 8B 81 D4 00 00 00");
		if (ViewportResolutionInstructions34ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 34 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions34ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions34ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction34MidHook{};
			
			ViewportResolutionWidthInstruction34MidHook = safetyhook::create_mid(ViewportResolutionInstructions34ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions34ScanResult + 28, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionHeightInstruction34MidHook{};
			
			ViewportResolutionHeightInstruction34MidHook = safetyhook::create_mid(ViewportResolutionInstructions34ScanResult + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 34 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions35ScanResult = Memory::PatternScan(exeModule, "8B 80 D0 00 00 00 99 2B C2 D1 F8 2D 90 01 00 00 89 86 8C 04 00 00 8B 0D 20 CD 54 00 8B 81 D4 00 00 00 99 2B C2 D1 F8 2D 2C 01 00 00 89 86 90 04 00 00 8B 46 04 3B C5 75 43");
		if (ViewportResolutionInstructions35ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 35 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions35ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionInstructions35ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction35MidHook{};

			ViewportResolutionWidthInstruction35MidHook = safetyhook::create_mid(ViewportResolutionInstructions35ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions35ScanResult + 28, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionHeightInstruction35MidHook{};

			ViewportResolutionHeightInstruction35MidHook = safetyhook::create_mid(ViewportResolutionInstructions35ScanResult + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 35 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions36ScanResult = Memory::PatternScan(exeModule, "8B 80 D0 00 00 00 99 2B C2 D1 F8 2D 90 01 00 00 89 86 8C 04 00 00 8B 0D 20 CD 54 00 8B 81 D4 00 00 00 99 2B C2 D1 F8 2D 2C 01 00 00 89 86 90 04 00 00 8B 46 04 3B C3 75 43");
		if (ViewportResolutionInstructions36ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 36 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions36ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionInstructions36ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction36MidHook{};

			ViewportResolutionWidthInstruction36MidHook = safetyhook::create_mid(ViewportResolutionInstructions36ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions36ScanResult + 28, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionHeightInstruction36MidHook{};

			ViewportResolutionHeightInstruction36MidHook = safetyhook::create_mid(ViewportResolutionInstructions36ScanResult + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 36 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions37ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 8B 90 D0 00 00 00 8B 46 04 D1 F9 D1 FA 81 E9 2C 01 00 00");
		if (ViewportResolutionInstructions37ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 37 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions37ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions37ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);
			
			static SafetyHookMid ViewportResolutionInstructions37MidHook{};
			
			ViewportResolutionInstructions37MidHook = safetyhook::create_mid(ViewportResolutionInstructions37ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 37 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction38ScanResult = Memory::PatternScan(exeModule, "8B 82 D0 00 00 00 D1 F8 89 44 24 18 DB 44 24 18 D8 25 FC C5 51 00 D9 1C 24 E8 5F 4B 0B 00");
		if (ViewportResolutionWidthInstruction38ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 38 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction38ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction38ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction38MidHook{};
			
			ViewportResolutionWidthInstruction38MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction38ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 38 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions39ScanResult = Memory::PatternScan(exeModule, "8B 90 D0 00 00 00 53 6A 01 53 53 8D 4E 1C D1 FA 6A 03");
		if (ViewportResolutionInstructions39ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 39 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions39ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionInstructions39ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction39MidHook{};
			
			ViewportResolutionWidthInstruction39MidHook = safetyhook::create_mid(ViewportResolutionInstructions39ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 39 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction40ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 53 8D 86 1C 01 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 6A 64");
		if (ViewportResolutionWidthInstruction40ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 40 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction40ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ViewportResolutionWidthInstruction40ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ViewportResolutionWidthInstruction40MidHook{};
			
			ViewportResolutionWidthInstruction40MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction40ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 40 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction41ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 53 8D 86 1C 02 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 87 00 00 00");
		if (ViewportResolutionWidthInstruction41ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 41 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction41ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction41ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction41MidHook{};

			ViewportResolutionWidthInstruction41MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction41ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 41 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction42ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 53 8D 86 1C 03 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 A0 00 00 00");
		if (ViewportResolutionWidthInstruction42ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 42 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction42ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction42ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction42MidHook{};

			ViewportResolutionWidthInstruction42MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction42ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 42 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction43ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 8B CF D1 FA 81 EA 77 01 00 00 52 E8 83 DA 00 00");
		if (ViewportResolutionWidthInstruction43ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 43 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction43ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction43ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction43MidHook{};

			ViewportResolutionWidthInstruction43MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction43ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 43 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction44ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 53 8D 86 1C 06 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 1D 01 00 00 52 8B CF");
		if (ViewportResolutionWidthInstruction44ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 44 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction44ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction44ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction44MidHook{};

			ViewportResolutionWidthInstruction44MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction44ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 44 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction45ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 53 8D 86 1C 07 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 36 01 00 00 52 8B CF E8 25 DA 00 00");
		if (ViewportResolutionWidthInstruction45ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 45 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction45ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction45ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction45MidHook{};

			ViewportResolutionWidthInstruction45MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction45ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 45 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction46ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 53 8D 86 1C 08 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 4F 01 00 00 52 8B CF E8 F6 D9 00 00 8B 0D 20 CD 54 00");
		if (ViewportResolutionWidthInstruction46ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 46 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction46ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction46ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction46MidHook{};

			ViewportResolutionWidthInstruction46MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction46ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 46 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction47ScanResult = Memory::PatternScan(exeModule, "8B 91 D0 00 00 00 53 8D 86 1C 09 00 00 6A 01 D1 FA 50 81 EA 77 01 00 00 68 68 01 00 00");
		if (ViewportResolutionWidthInstruction47ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 47 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction47ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction47ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction47MidHook{};

			ViewportResolutionWidthInstruction47MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction47ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 47 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionHeightInstruction48ScanResult = Memory::PatternScan(exeModule, "DB 80 D4 00 00 00 DA 64 24 14 D9 1C 24 DB 44 24 10 51 D9 1C 24 E8 47 5A 0B 00");
		if (ViewportResolutionHeightInstruction48ScanResult)
		{
			spdlog::info("Viewport Resolution Height Instruction 48 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction47ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionHeightInstruction48ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction48Hook = safetyhook::create_mid(ViewportResolutionHeightInstruction48ScanResult, ViewportResolutionHeightInstruction48MidHook);
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution height instruction 48 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionHeightInstruction49ScanResult = Memory::PatternScan(exeModule, "DB 81 D4 00 00 00 D8 64 24 30 D9 5C 24 1C 74 1D 8B 54 B7 08");
		if (ViewportResolutionHeightInstruction49ScanResult)
		{
			spdlog::info("Viewport Resolution Height Instruction 49 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionHeightInstruction49ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionHeightInstruction49ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionHeightInstruction49Hook = safetyhook::create_mid(ViewportResolutionHeightInstruction49ScanResult, ViewportResolutionHeightInstruction49MidHook);
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution height instruction 49 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionHeightInstruction50ScanResult = Memory::PatternScan(exeModule, "8B 88 D4 00 00 00 57 8B 96 FC 04 00 00 81 E9 91 00 00 00 51 6A 19 52 E8 9B 15 01 00");
		if (ViewportResolutionHeightInstruction50ScanResult)
		{
			spdlog::info("Viewport Resolution Height Instruction 50 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionHeightInstruction50ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionHeightInstruction50ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionHeightInstruction50MidHook{};

			ViewportResolutionHeightInstruction50MidHook = safetyhook::create_mid(ViewportResolutionHeightInstruction50ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution height instruction 50 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionHeightInstruction51ScanResult = Memory::PatternScan(exeModule, "8B 99 D4 00 00 00 81 EB 91 00 00 00 DF E0 F6 C4 41 75 15 8B 8E 00 05 00 00");
		if (ViewportResolutionHeightInstruction51ScanResult)
		{
			spdlog::info("Viewport Resolution Height Instruction 51 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionHeightInstruction51ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionHeightInstruction51ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionHeightInstruction51MidHook{};

			ViewportResolutionHeightInstruction51MidHook = safetyhook::create_mid(ViewportResolutionHeightInstruction51ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution height instruction 51 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionHeightInstruction52ScanResult = Memory::PatternScan(exeModule, "8B 91 D4 00 00 00 8B 4E 10 D9 1C 24 2B D1 51 2B D0 89 54 24 1C DB 44 24 1C D9 1C 24 DB 46 0C");
		if (ViewportResolutionHeightInstruction52ScanResult)
		{
			spdlog::info("Viewport Resolution Height Instruction 52 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionHeightInstruction52ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionHeightInstruction52ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionHeightInstruction52MidHook{};

			ViewportResolutionHeightInstruction52MidHook = safetyhook::create_mid(ViewportResolutionHeightInstruction52ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution height instruction 52 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions53ScanResult = Memory::PatternScan(exeModule, "8B 80 D0 00 00 00 99 2B C2 D1 F8 2D 90 01 00 00 89 86 8C 04 00 00 8B 0D 20 CD 54 00 8B 81 D4 00 00 00 99 2B C2 D1 F8 2D 2C 01 00 00 89 86 90 04 00 00 A1 5C CD 54 00");
		if (ViewportResolutionInstructions53ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 53 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions53ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionInstructions53ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction53MidHook{};

			ViewportResolutionWidthInstruction53MidHook = safetyhook::create_mid(ViewportResolutionInstructions53ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions53ScanResult + 28, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionHeightInstruction53MidHook{};

			ViewportResolutionHeightInstruction53MidHook = safetyhook::create_mid(ViewportResolutionInstructions53ScanResult + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 53 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionWidthInstruction54ScanResult = Memory::PatternScan(exeModule, "8B 88 D0 00 00 00 81 E9 82 00 00 00 51 52 8B CE E8 D2 93 FF FF 33 ED");
		if (ViewportResolutionWidthInstruction54ScanResult)
		{
			spdlog::info("Viewport Resolution Width Instruction 54 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionWidthInstruction54ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionWidthInstruction54ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ViewportResolutionWidthInstruction54MidHook{};

			ViewportResolutionWidthInstruction54MidHook = safetyhook::create_mid(ViewportResolutionWidthInstruction54ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution width instruction 54 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "C7 44 24 54 33 33 33 3F C7 44 24 4C CD CC 8C 3F ?? ?? ?? ?? ?? ?? 8D 83 28 0E 00 00 50 68 2C 01 00 00 68 F4 A7 52 00 68 E4 A4 52 00 D8 0D 0C C7 51 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			fNewCameraHFOV = (0.7f * fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstructionScanResult + 4, fNewCameraHFOV);

			CameraVFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionScanResult + 46, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult + 44, "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 44, CameraVFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
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