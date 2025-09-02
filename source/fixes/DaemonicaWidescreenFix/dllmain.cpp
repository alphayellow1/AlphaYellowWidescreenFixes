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
HMODULE dllModule2 = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "DaemonicaWidescreenFix";
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
float fNewCameraFOV;
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
uint8_t* ResolutionWidth2Address;
uint8_t* ResolutionHeight2Address;
uint8_t* ResolutionWidth3Address;
uint8_t* ResolutionHeight3Address;
uint8_t* ResolutionWidth4Address;
uint8_t* ResolutionHeight4Address;
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
uint8_t* CameraFOV1Address;
uint8_t* CameraFOV2Address;
uint8_t* CameraFOV3Address;

// Game detection
enum class Game
{
	DAEMONICA,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DAEMONICA, {"Daemonica", "maindata.dae"}},
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

static SafetyHookMid AspectRatioInstruction1Hook{};

void AspectRatioInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid AspectRatioInstruction2Hook{};

void AspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fdiv dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid CameraFOVInstruction1Hook{};

void CameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(CameraFOV1Address);

	fNewCameraFOV1 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV1, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV1]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(CameraFOV2Address);

	fNewCameraFOV2 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV2, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(CameraFOV3Address);

	fNewCameraFOV3 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV3, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV3]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::DAEMONICA && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 00 05 00 00 00 04 00 00 78 05 00 00 1A 04 00 00 40 06 00 00 B0 04 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);
			
			// 800x600
			Memory::Write(ResolutionListScanResult, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 4, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 12, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 16, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 20, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? A3 ?? ?? ?? ?? C1 E0 0A 89 0D ?? ?? ?? ?? 89 35 ?? ?? ?? ?? 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth2Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionWidth3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 6, Memory::PointerMode::Absolute);

			ResolutionHeight2Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 15, Memory::PointerMode::Absolute);

			ResolutionHeight3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 27, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10);

			static SafetyHookMid ResolutionWidthInstructions2MidHook{};

			ResolutionWidthInstructions2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth2Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionWidth3Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 13, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeight2Instruction1MidHook{};

			ResolutionHeight2Instruction1MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 13, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight2Address) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 25, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeight2Instruction2MidHook{};

			ResolutionHeight2Instruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 25, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight3Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "89 15 ?? ?? ?? ?? 8B 86 B0 00 00 00 A3 ?? ?? ?? ?? 8B C7 5F 5D 5B");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 13, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction3MidHook{};

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth4Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions3ScanResult + 12, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction3MidHook{};

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight4Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "89 86 AC 00 00 00 2B CA 89 46 68 8B 86 00 02 00 00 85 C0 89 8E B0 00 00 00 89 4E 6C");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidth1Instruction4MidHook{};

			ResolutionWidth1Instruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xAC) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 8, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidth2Instruction4MidHook{};

			ResolutionWidth2Instruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 8, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x68) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 19, "\x90\x90\x90\x90\x90\x90\x90\x90\x90", 9);

			static SafetyHookMid ResolutionHeightInstructions4MidHook{};

			ResolutionHeightInstructions4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 19, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xB0) = iCurrentResY;

				*reinterpret_cast<int*>(ctx.esi + 0x6C) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 4 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "89 B7 D0 01 00 00 89 9F D4 01 00 00 C7 05 ?? ?? ?? ?? 3C 00 00 00");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions5ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions5MidHook{};

			ResolutionInstructions5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi + 0x1D0) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edi + 0x1D4) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 8B 44 24 24 85 C0 74 05 A3 ?? ?? ?? ??");
		if (ResolutionInstructions6ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 14, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions6ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction6MidHook{};

			ResolutionWidthInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth6Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 13, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction6MidHook{};

			ResolutionHeightInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult + 13, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight6Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 6 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions7ScanResult = Memory::PatternScan(exeModule, "89 35 ?? ?? ?? ?? 8B 41 24 85 C0 75 15 8B 0D ?? ?? ?? ?? 33 C0 A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions7ScanResult)
		{
			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions7ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions7ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions7ScanResult + 28, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions7ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction7MidHook{};

			ResolutionWidthInstruction7MidHook = safetyhook::create_mid(ResolutionInstructions7ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth7Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions7ScanResult + 26, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction7MidHook{};

			ResolutionHeightInstruction7MidHook = safetyhook::create_mid(ResolutionInstructions7ScanResult + 26, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight7Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 7 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions8ScanResult = Memory::PatternScan(exeModule, "89 35 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 89 3D ?? ?? ?? ?? 7D 04 33 C9 EB 09 8D 43 FF");
		if (ResolutionInstructions8ScanResult)
		{
			spdlog::info("Resolution Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions8ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions8ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions8ScanResult + 14, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions8ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction8MidHook{};

			ResolutionWidthInstruction8MidHook = safetyhook::create_mid(ResolutionInstructions8ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth8Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions8ScanResult + 12, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction8MidHook{};

			ResolutionHeightInstruction8MidHook = safetyhook::create_mid(ResolutionInstructions8ScanResult + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight8Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 8 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions9ScanResult = Memory::PatternScan(exeModule, "89 35 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 5E C3 CC CC CC CC CC CC CC");
		if (ResolutionInstructions9ScanResult)
		{
			spdlog::info("Resolution Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions9ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth9Address = Memory::GetPointer<uint32_t>(ResolutionInstructions9ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight9Address = Memory::GetPointer<uint32_t>(ResolutionInstructions9ScanResult + 19, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions9ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction9MidHook{};

			ResolutionWidthInstruction9MidHook = safetyhook::create_mid(ResolutionInstructions9ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth9Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions9ScanResult + 17, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction9MidHook{};

			ResolutionHeightInstruction9MidHook = safetyhook::create_mid(ResolutionInstructions9ScanResult + 17, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight9Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 9 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions10ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 2B CA 89 15 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? C7 05 ?? ?? ?? ?? 00 00 00 00 C7 05 ?? ?? ?? ?? 00 00 80 3F");
		if (ResolutionInstructions10ScanResult)
		{
			spdlog::info("Resolution Instructions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions10ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth10Address = Memory::GetPointer<uint32_t>(ResolutionInstructions10ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight10Address = Memory::GetPointer<uint32_t>(ResolutionInstructions10ScanResult + 20, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions10ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction10MidHook{};

			ResolutionWidthInstruction10MidHook = safetyhook::create_mid(ResolutionInstructions10ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth10Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions10ScanResult + 18, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction10MidHook{};

			ResolutionHeightInstruction10MidHook = safetyhook::create_mid(ResolutionInstructions10ScanResult + 18, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight10Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 10 scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? DD 1C 24 E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 8B 02 83 C4 10 3B C3");
		if (AspectRatioInstruction1ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction1ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(AspectRatioInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstruction1ScanResult, AspectRatioInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 5C 24 08 89 0D ?? ?? ?? ?? D8 74 24 08 D9 1D ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 35 ?? ?? ?? ??");
		if (AspectRatioInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstruction2ScanResult + 26, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstruction2ScanResult + 26, AspectRatioInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? DD 1C 24 E8 ?? ?? ?? ?? D9 5C 24 20 83 C4 08 D9 44 24 18");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			CameraFOV1Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction1ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, CameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "74 5E D9 05 ?? ?? ?? ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 05 7A 1C");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			CameraFOV2Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction2ScanResult + 4, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult + 2, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 2, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "DC C0 DD 1C 24 E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			CameraFOV3Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction3ScanResult + 18, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult + 16, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult + 16, CameraFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 3 memory address.");
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