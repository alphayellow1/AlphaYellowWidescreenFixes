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
std::string sFixName = "AntzExtremeRacingWidescreenFix";
std::string sFixVersion = "1.4";
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
constexpr float fOriginalMenuCameraFOV = 0.5f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewMenuFOV;
float fFOVFactor;
float fValue1;
float fValue2;
float fValue3;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fNewCameraFOV3;
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
uint8_t* ResolutionWidth9Address;
uint8_t* ResolutionHeight9Address;
uint8_t* ResolutionWidth10Address;
uint8_t* ResolutionHeight10Address;
uint8_t* ResolutionWidth11Address;
uint8_t* ResolutionHeight11Address;
uint8_t* ResolutionWidth12Address;
uint8_t* ResolutionHeight12Address;
uint8_t* ResolutionWidth13Address;
uint8_t* ResolutionHeight13Address;
uint8_t* ResolutionWidth14Address;
uint8_t* ResolutionHeight14Address;

// Game detection
enum class Game
{
	AER,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AER, {"Antz Extreme Racing", "antzextremeracing.exe"}},
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

static SafetyHookMid CameraHFOVInstructionHook{};

void CameraHFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.eax + 0x3C);

	fNewCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV]
	}
}

static SafetyHookMid CameraVFOVInstructionHook{};

void CameraVFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fNewCameraVFOV = fNewCameraHFOV / fNewAspectRatio;

	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::AER && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "89 0D 00 1D CD 00 89 15 04 1D CD 00 C7 05 14 1D CD 00 02 00 00 00");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth1Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight1Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 8, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions1MidHook{};

			ResolutionInstructions1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "A3 00 1D CD 00 41 89 0D 04 1D CD 00");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth2Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight2Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 8, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth2Address) = iCurrentResX;				
			});

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 6, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight2Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "66 A3 ?? ?? ?? ?? 66 8B 0D ?? ?? ?? ?? 66 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 16, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction3MidHook{};

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth3Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions3ScanResult + 13, "\x90\x90\x90\x90\x90\x90\x90", 7);

			static SafetyHookMid ResolutionHeightInstruction3MidHook{};

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult + 13, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight3Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "66 C7 05 ?? ?? ?? ?? 80 02 66 C7 05 ?? ?? ?? ?? E0 01");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructions5ScanResult + 7, iCurrentResX);

			Memory::Write(ResolutionInstructions5ScanResult + 16, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 8B 55 ?? 89 15 ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? E9");
		if (ResolutionInstructions6ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 11, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions6ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction6MidHook{};

			ResolutionWidthInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth6Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 9, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction6MidHook{};

			ResolutionHeightInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult + 9, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight6Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 6 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions7ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 8B 45 F0 A3 ?? ?? ?? ??");
		if (ResolutionInstructions7ScanResult)
		{
			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions7ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions7ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions7ScanResult + 9, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions7ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction7MidHook{};

			ResolutionWidthInstruction7MidHook = safetyhook::create_mid(ResolutionInstructions7ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth7Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions7ScanResult + 8, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction7MidHook{};

			ResolutionHeightInstruction7MidHook = safetyhook::create_mid(ResolutionInstructions7ScanResult + 8, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight7Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 7 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions8ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 8B 4D ?? 89 0D ?? ?? ?? ?? 8B 55 ?? 89 15 ?? ?? ?? ?? C7 05");
		if (ResolutionInstructions8ScanResult)
		{
			spdlog::info("Resolution Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions8ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions8ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions8ScanResult + 10, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions8ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction8MidHook{};

			ResolutionWidthInstruction8MidHook = safetyhook::create_mid(ResolutionInstructions8ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth8Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions8ScanResult + 8, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction8MidHook{};

			ResolutionHeightInstruction8MidHook = safetyhook::create_mid(ResolutionInstructions8ScanResult + 8, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight8Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 8 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions9ScanResult = Memory::PatternScan(exeModule, "89 15 ?? ?? ?? ?? 8B 45 ?? A3 ?? ?? ?? ?? 8B 4D ?? 89 0D ?? ?? ?? ?? C7 05");
		if (ResolutionInstructions9ScanResult)
		{
			spdlog::info("Resolution Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions9ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth9Address = Memory::GetPointer<uint32_t>(ResolutionInstructions9ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight9Address = Memory::GetPointer<uint32_t>(ResolutionInstructions9ScanResult + 10, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions9ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction9MidHook{};

			ResolutionWidthInstruction9MidHook = safetyhook::create_mid(ResolutionInstructions9ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth9Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions9ScanResult + 9, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction9MidHook{};

			ResolutionHeightInstruction9MidHook = safetyhook::create_mid(ResolutionInstructions9ScanResult + 9, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight9Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 9 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions10ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 0F BF 0D ?? ?? ?? ?? 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions10ScanResult)
		{
			spdlog::info("Resolution Instructions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions10ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth10Address = Memory::GetPointer<uint32_t>(ResolutionInstructions10ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight10Address = Memory::GetPointer<uint32_t>(ResolutionInstructions10ScanResult + 14, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions10ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction10MidHook{};

			ResolutionWidthInstruction10MidHook = safetyhook::create_mid(ResolutionInstructions10ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth10Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions10ScanResult + 12, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction10MidHook{};

			ResolutionHeightInstruction10MidHook = safetyhook::create_mid(ResolutionInstructions10ScanResult + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight10Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 10 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions11ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 8B 10 FF 52 ?? 3B C3 0F 8C ?? ?? ?? ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 8B 08 FF 91 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 8B 10 FF 52 ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 8B 08 FF 91 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 8B 10 FF 52 ?? E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 33 C0 F7 C1 ?? ?? ?? ?? 74 ?? 8B 35 ?? ?? ?? ?? 3B F3 74 ?? 8B 4E ?? 8B 6E ?? 8D 7E ?? 3B CB 75 ?? 8B 16 A1 ?? ?? ?? ?? 57 53 8B 08 52 8B 56 ?? 68 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 76 ?? 3B F3 74 ?? 8B 0F 89 0E 3B C3 0F 8C ?? ?? ?? ?? 3B EB 8B F5 75 ?? 8B 0D ?? ?? ?? ?? 33 C0 3B CB 74 ?? 8B 51 ?? 8B 71 ?? 3B D3 74 ?? 39 1A 75 ?? 8B 69 ?? A1 ?? ?? ?? ?? 55 52 8B 51 ?? 8B 09 8B 38 52 51 50 FF 97 ?? ?? ?? ?? 3B C3 7C ?? 3B F3 8B CE 75 ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 33 F6 3B C3 7E ?? 33 FF 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 03 CF 8B 10 51 56 50 FF 92 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 4C 39");
		if (ResolutionInstructions11ScanResult)
		{
			spdlog::info("Resolution Instructions 11 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions11ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth11Address = Memory::GetPointer<uint32_t>(ResolutionInstructions11ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight11Address = Memory::GetPointer<uint32_t>(ResolutionInstructions11ScanResult + 7, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions11ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid ResolutionInstructions11MidHook{};

			ResolutionInstructions11MidHook = safetyhook::create_mid(ResolutionInstructions11ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth11Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight11Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 11 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions12ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 08 50 FF 51 ?? 85 C0 0F 8C ?? ?? ?? ?? 8B 54 24");
		if (ResolutionInstructions12ScanResult)
		{
			spdlog::info("Resolution Instructions 12 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions12ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth12Address = Memory::GetPointer<uint32_t>(ResolutionInstructions12ScanResult + 12, Memory::PointerMode::Absolute);

			ResolutionHeight12Address = Memory::GetPointer<uint32_t>(ResolutionInstructions12ScanResult + 1, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions12ScanResult + 10, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction12MidHook{};

			ResolutionWidthInstruction12MidHook = safetyhook::create_mid(ResolutionInstructions12ScanResult + 10, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth12Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions12ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction12MidHook{};

			ResolutionHeightInstruction12MidHook = safetyhook::create_mid(ResolutionInstructions12ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight12Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 12 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions13ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 8B 10 FF 52 ?? 3B C3 0F 8C ?? ?? ?? ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 8B 08 FF 91 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 8B 10 FF 52 ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 8B 08 FF 91 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 8B 10 FF 52 ?? E8 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 33 C0 F7 C1 ?? ?? ?? ?? 74 ?? 8B 35 ?? ?? ?? ?? 3B F3 74 ?? 8B 4E ?? 8B 6E ?? 8D 7E ?? 3B CB 75 ?? 8B 16 A1 ?? ?? ?? ?? 57 53 8B 08 52 8B 56 ?? 68 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 76 ?? 3B F3 74 ?? 8B 0F 89 0E 3B C3 0F 8C ?? ?? ?? ?? 3B EB 8B F5 75 ?? 8B 0D ?? ?? ?? ?? 33 C0 3B CB 74 ?? 8B 51 ?? 8B 71 ?? 3B D3 74 ?? 39 1A 75 ?? 8B 69 ?? A1 ?? ?? ?? ?? 55 52 8B 51 ?? 8B 09 8B 38 52 51 50 FF 97 ?? ?? ?? ?? 3B C3 7C ?? 3B F3 8B CE 75 ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 33 F6 3B C3 7E ?? 33 FF 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 03 CF 8B 10 51 56 50 FF 92 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 4C 0F");
		if (ResolutionInstructions13ScanResult)
		{
			spdlog::info("Resolution Instructions 13 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions13ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth13Address = Memory::GetPointer<uint32_t>(ResolutionInstructions13ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight13Address = Memory::GetPointer<uint32_t>(ResolutionInstructions13ScanResult + 7, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions13ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid ResolutionInstructions13MidHook{};

			ResolutionInstructions13MidHook = safetyhook::create_mid(ResolutionInstructions13ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth13Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight13Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 13 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions14ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 08 50 FF 51 ?? 85 C0 0F 8C ?? ?? ?? ?? 8B 15");
		if (ResolutionInstructions14ScanResult)
		{
			spdlog::info("Resolution Instructions 14 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions14ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth14Address = Memory::GetPointer<uint32_t>(ResolutionInstructions14ScanResult + 12, Memory::PointerMode::Absolute);

			ResolutionHeight14Address = Memory::GetPointer<uint32_t>(ResolutionInstructions14ScanResult + 1, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions14ScanResult + 10, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction14MidHook{};

			ResolutionWidthInstruction14MidHook = safetyhook::create_mid(ResolutionInstructions14ScanResult + 10, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth14Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions14ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction14MidHook{};

			ResolutionHeightInstruction14MidHook = safetyhook::create_mid(ResolutionInstructions14ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight14Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 14 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "8B 4E ?? 89 54 24 ?? 8B 56 ?? 89 44 24 ?? A1");
		if (ViewportResolutionInstructions1ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionInstructions1ScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid ViewportWidthResolutionInstruction1MidHook{};

			ViewportWidthResolutionInstruction1MidHook = safetyhook::create_mid(ViewportResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions1ScanResult + 7, "\x90\x90\x90", 3);

			static SafetyHookMid ViewportHeightResolutionInstruction1MidHook{};

			ViewportHeightResolutionInstruction1MidHook = safetyhook::create_mid(ViewportResolutionInstructions1ScanResult + 7, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ViewportResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "8B 50 ?? 89 54 24 ?? 8B 40 ?? 89 44 24 ?? A1");
		if (ViewportResolutionInstructions2ScanResult)
		{
			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ViewportResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ViewportResolutionInstructions2ScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid ViewportWidthResolutionInstruction2MidHook{};

			ViewportWidthResolutionInstruction2MidHook = safetyhook::create_mid(ViewportResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ViewportResolutionInstructions2ScanResult + 7, "\x90\x90\x90", 3);

			static SafetyHookMid ViewportHeightResolutionInstruction2MidHook{};

			ViewportHeightResolutionInstruction2MidHook = safetyhook::create_mid(ViewportResolutionInstructions2ScanResult + 7, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate viewport resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 40 3C D8 49 28 D9 5D F8 8B 55 08 8B 45 08 D9 42 40");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraHFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionScanResult + 15, "\x90\x90\x90", 3);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 15, CameraVFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		fNewMenuFOV = Maths::CalculateNewFOV_MultiplierBased(fOriginalMenuCameraFOV, fAspectRatioScale);

		std::uint8_t* MenuAspectRatioAndFOVScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A 00 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 10 68 6C 34 58 00 E8 00 1F 00 00 83 C4 04");
		if (MenuAspectRatioAndFOVScanResult)
		{
			spdlog::info("Menu Aspect Ratio & FOV: Address is {:s}+{:x}", sExeName.c_str(), MenuAspectRatioAndFOVScanResult - (std::uint8_t*)exeModule);

			Memory::Write(MenuAspectRatioAndFOVScanResult + 1, fNewAspectRatio);

			Memory::Write(MenuAspectRatioAndFOVScanResult + 6, fNewMenuFOV);
		}
		else
		{
			spdlog::error("Failed to locate menu aspect ratio & FOV scan 1 memory address.");
			return;
		}

		std::uint8_t* MenuAspectRatioAndFOV2ScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A 00 68 AC CD BC 00 E8 2C 8F F8 FF 83 C4 10 8B 0D A0 7A 57 00");
		if (MenuAspectRatioAndFOV2ScanResult)
		{
			spdlog::info("Menu Aspect Ratio & FOV 2: Address is {:s}+{:x}", sExeName.c_str(), MenuAspectRatioAndFOV2ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(MenuAspectRatioAndFOV2ScanResult + 1, fNewAspectRatio);

			Memory::Write(MenuAspectRatioAndFOV2ScanResult + 6, fNewMenuFOV);
		}
		else
		{
			spdlog::error("Failed to locate menu aspect ratio & FOV 2 scan memory address.");
			return;
		}

		std::uint8_t* Value1ScanResult = Memory::PatternScan(exeModule, "C7 40 28 CD CC CC 3F 6A 00 8B 4D 08 8B 51 14");
		if (Value1ScanResult)
		{
			spdlog::info("Value 1: Address is {:s}+{:x}", sExeName.c_str(), Value1ScanResult - (std::uint8_t*)exeModule);

			fValue1 = 1.6f * fAspectRatioScale;

			Memory::Write(Value1ScanResult + 3, fValue1);
		}
		else
		{
			spdlog::error("Failed to locate value 1 memory address.");
			return;
		}

		std::uint8_t* Value2ScanResult = Memory::PatternScan(exeModule, "C7 45 F4 00 00 80 3F E9 A0 01 00 00");
		if (Value2ScanResult)
		{
			spdlog::info("Value 2: Address is {:s}+{:x}", sExeName.c_str(), Value2ScanResult - (std::uint8_t*)exeModule);

			fValue2 = 1.0f * fAspectRatioScale;

			Memory::Write(Value2ScanResult + 3, fValue2);
		}
		else
		{
			spdlog::error("Failed to locate value 2 memory address.");
			return;
		}

		std::uint8_t* Value3ScanResult = Memory::PatternScan(exeModule, "C7 45 F4 00 00 80 3F C7 05 30 D2 C0 00 00 00 00 00");
		if (Value3ScanResult)
		{
			spdlog::info("Value 3: Address is {:s}+{:x}", sExeName.c_str(), Value3ScanResult - (std::uint8_t*)exeModule);

			fValue3 = 1.0f * fAspectRatioScale;

			Memory::Write(Value3ScanResult + 3, fValue3);
		}
		else
		{
			spdlog::error("Failed to locate value 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "8B 8C 11 FC 7A 57 00 89 48 20 8B 55 0C 6B D2 70");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90\x90", 7);

			static SafetyHookMid CameraFOVInstruction3MidHook{};

			CameraFOVInstruction3MidHook = safetyhook::create_mid(CameraFOVInstruction3ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(ctx.ecx + ctx.edx + 0x00577AFC);

				fNewCameraFOV3 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV3, fAspectRatioScale);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV3);
			});
		}
		else
		{
			spdlog::error("Failed to locate value 4 memory address.");
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
