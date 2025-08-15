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
HMODULE thisModule;

// Fix details
std::string sFixName = "Sydney2000FOVFix";
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

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewAspectRatio2;
float fFOVFactor;
float fAspectRatioScale;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
float fNewCameraFOV5;
float fNewCameraFOV6;
float fNewCameraFOV7;
float fNewCameraFOV8;
float fNewCameraFOV9;
float fNewCameraFOV10;
float fNewCameraFOV11;
float fNewCameraFOV12;
float fNewCameraFOV13;
float fNewCameraFOV14;
float fNewCameraFOV15;
float fNewCameraFOV16;
float fNewCameraFOV17;
float fNewCameraFOV18;
float fNewCameraFOV19;
float fNewCameraFOV20;
float fNewCameraFOV21;
float fNewCameraFOV22;
float fNewCameraFOV23;

// Game detection
enum class Game
{
	SYDNEY2000,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SYDNEY2000, {"Sydney 2000", "Olympics.exe"}},
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

static SafetyHookMid CameraFOVInstruction16Hook{};

void CameraFOVInstruction16MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV16 = *reinterpret_cast<float*>(ctx.ecx + 0xBC);

	fNewCameraFOV16 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV16, fAspectRatioScale) * fFOVFactor;
}

static SafetyHookMid CameraFOVInstruction17Hook{};

void CameraFOVInstruction17MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV17 = *reinterpret_cast<float*>(ctx.ecx + 0xBC);

	fNewCameraFOV17 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV17, fAspectRatioScale) * fFOVFactor;
}

void FOVFix()
{
	if (eGameType == Game::SYDNEY2000 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "89 96 BC 00 00 00 8B 07 F6 C4 01 74 09 8B 47 58");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction1MidHook{};

			CameraFOVInstruction1MidHook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV1 = std::bit_cast<float>(ctx.edx);

				fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.esi + 0xBC) = fNewCameraFOV1;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "89 91 BC 00 00 00 8B 10 F6 C6 01 74 09 8B 40 58 89 81 C0 00 00 00 C2 04 00 90 90 90 90 83 EC 18 53 8B 5C 24 20 56 8B F1 8B 43 04 8B 4E 14 2B C1 8B 0B C1 F8 08");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction2MidHook{};

			CameraFOVInstruction2MidHook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV2 = std::bit_cast<float>(ctx.edx);

				fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.ecx + 0xBC) = fNewCameraFOV2;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "C7 81 BC 00 00 00 91 0A 06 3F C3 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV3 = Maths::CalculateNewFOV_RadBased(0.5235987305641174f, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstruction3ScanResult + 6, fNewCameraFOV3);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "89 91 BC 00 00 00 8B 10 F6 C6 01 74 09 8B 40 58 89 81 C0 00 00 00 C2 04 00 90 90 90 90 90 90 90 90 90 90 90 90 90 90 83 EC 0C 56 57 8B 7C 24 18 8B F1 8B 07 83 E8 03 0F 84 88 00 00 00 83 E8 02");
		if (CameraFOVInstruction4ScanResult)
		{
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction4ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction4MidHook{};

			CameraFOVInstruction4MidHook = safetyhook::create_mid(CameraFOVInstruction4ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV4 = std::bit_cast<float>(ctx.edx);

				fNewCameraFOV4 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV4, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.ecx + 0xBC) = fNewCameraFOV4;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction5ScanResult = Memory::PatternScan(exeModule, "89 8F BC 00 00 00 8B 06 F6 C4 01 74 09 8B 56 58 89 97 C0 00 00 00 5F 5E C2 04 00 90 90 90 56 57 8B 7C 24 0C");
		if (CameraFOVInstruction5ScanResult)
		{
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction5ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction5ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction5MidHook{};

			CameraFOVInstruction5MidHook = safetyhook::create_mid(CameraFOVInstruction5ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV5 = std::bit_cast<float>(ctx.ecx);

				fNewCameraFOV5 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV5, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.edi + 0xBC) = fNewCameraFOV5;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 5 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction6ScanResult = Memory::PatternScan(exeModule, "C7 86 BC 00 00 00 F1 66 5F 3F C7 86 50 01 00 00 68 11 03 42 C7 86 58 01 00 00 7F 42 0B C5 C7 86 5C 01 00 00 7F 42 0B 45 C6 86 45 01 00 00 03 88 86 46 01 00 00");
		if (CameraFOVInstruction6ScanResult)
		{
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction6ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV6 = Maths::CalculateNewFOV_RadBased(0.8726645112037659f, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstruction6ScanResult + 6, fNewCameraFOV6);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 6 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction7ScanResult = Memory::PatternScan(exeModule, "C7 86 BC 00 00 00 4E 77 56 3F 89 86 7C 01 00 00 89 86 80 01 00 00");
		if (CameraFOVInstruction7ScanResult)
		{
			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction7ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV7 = Maths::CalculateNewFOV_RadBased(0.83775794506073f, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstruction7ScanResult + 6, fNewCameraFOV7);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 7 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction8ScanResult = Memory::PatternScan(exeModule, "89 97 C4 00 00 00 8B 06 F6 C4 02 74 09 8B 46 5C 89 87 BC 00 00 00");
		if (CameraFOVInstruction8ScanResult)
		{
			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction8ScanResult + 16 - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction8ScanResult + 16, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction8MidHook{};

			CameraFOVInstruction8MidHook = safetyhook::create_mid(CameraFOVInstruction8ScanResult + 16, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV8 = std::bit_cast<float>(ctx.eax);

				fNewCameraFOV8 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV8, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.edi + 0xBC) = fNewCameraFOV8;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 8 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction9ScanResult = Memory::PatternScan(exeModule, "C7 86 BC 00 00 00 DB 0F C9 3F 89 BE D4 00 00 00 89 BE 80 00 00 00 C7 86 84 00 00 00 01 00 00 00 C7 46 04 02 00 00 00");
		if (CameraFOVInstruction9ScanResult)
		{
			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction9ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV9 = Maths::CalculateNewFOV_RadBased(1.5707963705062866f, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstruction9ScanResult + 6, fNewCameraFOV9);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 9 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction10ScanResult = Memory::PatternScan(exeModule, "89 91 BC 00 00 00 8B 10 F6 C6 01 74 09 8B 40 58 89 81 C0 00 00 00 C2 04 00 90 90 90 90 90 90 90 56 57 8B 7C 24 0C 8B F1 8B 07 83 E8 03 74 4F 83 E8 02 74 11");
		if (CameraFOVInstruction10ScanResult)
		{
			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction10ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction10ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction10MidHook{};

			CameraFOVInstruction10MidHook = safetyhook::create_mid(CameraFOVInstruction10ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV10 = std::bit_cast<float>(ctx.edx);

				fNewCameraFOV10 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV10, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.ecx + 0xBC) = fNewCameraFOV10;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 10 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction11ScanResult = Memory::PatternScan(exeModule, "89 91 BC 00 00 00 8B 10 F6 C6 01 74 09 8B 40 58 89 81 C0 00 00 00 C2 04 00 90 90 90 90 90 90 90 90 90 90 90 90 90 90 83 EC 30 56 57 8B 7C 24 3C");
		if (CameraFOVInstruction11ScanResult)
		{
			spdlog::info("Camera FOV Instruction 11: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction11ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction11ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction11MidHook{};

			CameraFOVInstruction11MidHook = safetyhook::create_mid(CameraFOVInstruction11ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV11 = std::bit_cast<float>(ctx.edx);

				fNewCameraFOV11 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV11, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.ecx + 0xBC) = fNewCameraFOV11;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 11 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction12ScanResult = Memory::PatternScan(exeModule, "89 86 BC 00 00 00 8B 07 F6 C4 04 74 0B 66 8B 4F 64 66 89 8E 8C 00 00 00 8B 07 F6 C4 08 74 09 8B 57 60 89 96 C4 00 00 00 8B 07 F6 C4 01 74 09 8B 47 58 89 86 C0 00 00 00 5F 5E 5B C2 04 00 90 90 90 90 90 90 90 90 56 57 8B 7C 24 0C 8B F1 8B 57 04 8B 4E 14 8B C2 2B C1");
		if (CameraFOVInstruction12ScanResult)
		{
			spdlog::info("Camera FOV Instruction 12: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction12ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction12ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction12MidHook{};

			CameraFOVInstruction12MidHook = safetyhook::create_mid(CameraFOVInstruction12ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV12 = std::bit_cast<float>(ctx.eax);

				fNewCameraFOV12 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV12, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.esi + 0xBC) = fNewCameraFOV12;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 12 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction13ScanResult = Memory::PatternScan(exeModule, "89 8E BC 00 00 00 8B 07 F6 C4 01 74 09 8B 57 58 89 96 C0 00 00 00 5F 5E C2 04 00 90 56 57 8B 7C 24 0C 8B F1 8B 57 04 8B 4E 14 8B C2 2B C1 8B 0F C1 F8 08 83 E9 03 0F 84 89 00 00 00 83 E9 02 74 11 68 B4 C1 54 00 6A 20 E8 EF 18 FB FF 83 C4 08 EB 73 85 C0 0F 84 9C 00 00 00");
		if (CameraFOVInstruction13ScanResult)
		{
			spdlog::info("Camera FOV Instruction 13: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction13ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction13ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction13MidHook{};

			CameraFOVInstruction13MidHook = safetyhook::create_mid(CameraFOVInstruction13ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV13 = std::bit_cast<float>(ctx.ecx);

				fNewCameraFOV13 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV13, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.esi + 0xBC) = fNewCameraFOV13;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 13 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction14ScanResult = Memory::PatternScan(exeModule, "D8 1D 04 03 53 00 DF E0 F6 C4 41 75 0A C7 81 BC 00 00 00 35 8D 27 40");
		if (CameraFOVInstruction14ScanResult)
		{
			spdlog::info("Camera FOV Instruction 14: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction14ScanResult + 13 - (std::uint8_t*)exeModule);

			fNewCameraFOV14 = Maths::CalculateNewFOV_RadBased(2.6179935932159424f, fAspectRatioScale) * fFOVFactor;
				
			Memory::Write(CameraFOVInstruction14ScanResult + 19, fNewCameraFOV14);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 14 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction15ScanResult = Memory::PatternScan(exeModule, "89 8E BC 00 00 00 8B 07 F6 C4 01 74 09 8B 57 58 89 96 C0 00 00 00 5F 5E C2 04 00 90 90 90 90 90 90 90 90 90 90 90 56");
		if (CameraFOVInstruction15ScanResult)
		{
			spdlog::info("Camera FOV Instruction 15: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction15ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction15ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction15MidHook{};

			CameraFOVInstruction15MidHook = safetyhook::create_mid(CameraFOVInstruction15ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV15 = std::bit_cast<float>(ctx.ecx);

				fNewCameraFOV15 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV15, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.esi + 0xBC) = fNewCameraFOV15;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 15 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction16ScanResult = Memory::PatternScan(exeModule, "D9 99 BC 00 00 00 DD D8 D9 81 BC 00 00 00");
		if (CameraFOVInstruction16ScanResult)
		{
			spdlog::info("Camera FOV Instruction 16: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction16ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction16ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction16Hook = safetyhook::create_mid(CameraFOVInstruction16ScanResult, CameraFOVInstruction16MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 16 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction17ScanResult = Memory::PatternScan(exeModule, "D9 99 BC 00 00 00 25 FD FF 00 00 66 89 81 8C 00 00 00");
		if (CameraFOVInstruction17ScanResult)
		{
			spdlog::info("Camera FOV Instruction 17: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction17ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction17ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction17Hook = safetyhook::create_mid(CameraFOVInstruction17ScanResult, CameraFOVInstruction17MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 17 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction18ScanResult = Memory::PatternScan(exeModule, "89 8E BC 00 00 00 8B 07 F6 C4 01 74 09 8B 57 58 89 96 C0 00 00 00 5F 5E C2 04 00 90 90 90 90 90 90 90 56 57 8B 7C 24 0C 8B F1 8B 57 04 8B 4E 14 8B C2 2B C1 8B 0F C1 F8 08 83 E9 03 74 57 83 E9 02 74 11");
		if (CameraFOVInstruction18ScanResult)
		{
			spdlog::info("Camera FOV Instruction 18: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction18ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction18ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction18MidHook{};

			CameraFOVInstruction18MidHook = safetyhook::create_mid(CameraFOVInstruction18ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV18 = std::bit_cast<float>(ctx.ecx);

				fNewCameraFOV18 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV18, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.esi + 0xBC) = fNewCameraFOV18;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 18 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction19ScanResult = Memory::PatternScan(exeModule, "C7 86 BC 00 00 00 A9 61 9C 3F C7 86 50 01 00 00 68 11 03 42 C7 86 58 01 00 00 A4 C0 F5 C4 C7 86 5C 01 00 00 C3 D5 A3 43");
		if (CameraFOVInstruction19ScanResult)
		{
			spdlog::info("Camera FOV Instruction 19: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction19ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV19 = Maths::CalculateNewFOV_RadBased(1.221730351448059f, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstruction19ScanResult + 6, fNewCameraFOV19);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 19 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction20ScanResult = Memory::PatternScan(exeModule, "89 8E BC 00 00 00 8B 07 F6 C4 04 74 0B 66 8B 57 64 66 89 96 8C 00 00 00 8B 07 F6 C4 08 74 09 8B 47 60 89 86 C4 00 00 00 8B 07 F6 C4 01 74 09 8B 4F 58");
		if (CameraFOVInstruction20ScanResult)
		{
			spdlog::info("Camera FOV Instruction 20: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction20ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction20ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction20MidHook{};

			CameraFOVInstruction20MidHook = safetyhook::create_mid(CameraFOVInstruction20ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV20 = std::bit_cast<float>(ctx.ecx);

				fNewCameraFOV20 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV20, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.esi + 0xBC) = fNewCameraFOV20;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 20 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction21ScanResult = Memory::PatternScan(exeModule, "C7 86 88 01 00 00 DB 0F C9 3F 8B 17 56 8B CF FF 52 04 6A 01 56");
		if (CameraFOVInstruction21ScanResult)
		{
			spdlog::info("Camera FOV Instruction 21: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction21ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV21 = Maths::CalculateNewFOV_RadBased(1.5707963705062866f, fAspectRatioScale);

			Memory::Write(CameraFOVInstruction21ScanResult + 6, fNewCameraFOV21);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 21 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction22ScanResult = Memory::PatternScan(exeModule, "89 86 BC 00 00 00 8B 03 F6 C4 04 74 0B 66 8B 4B 64 66 89 8E 8C 00 00 00 8B 03 F6 C4 08 74 09 8B 53 60 89 96 C4 00 00 00");
		if (CameraFOVInstruction22ScanResult)
		{
			spdlog::info("Camera FOV Instruction 22: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction22ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction22ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstruction22MidHook{};

			CameraFOVInstruction22MidHook = safetyhook::create_mid(CameraFOVInstruction22ScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV22 = std::bit_cast<float>(ctx.eax);

				fNewCameraFOV22 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV22, fAspectRatioScale) * fFOVFactor;

				*reinterpret_cast<float*>(ctx.esi + 0xBC) = fNewCameraFOV22;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 22 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction23ScanResult = Memory::PatternScan(exeModule, "C7 86 BC 00 00 00 4E 77 56 3F 89 86 40 01 00 00 33 C9 D9 80 0C 05 00 00 D9 96 44 01 00 00 D9 9E 4C 01 00 00");
		if (CameraFOVInstruction23ScanResult)
		{
			spdlog::info("Camera FOV Instruction 23: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction23ScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV23 = Maths::CalculateNewFOV_RadBased(0.83775794506073f, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstruction23ScanResult + 6, fNewCameraFOV23);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 23 memory address.");
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