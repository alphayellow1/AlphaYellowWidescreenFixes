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
std::string sFixName = "PetzHorsez2WidescreenFix";
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
constexpr float fTolerance = 0.000001f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewHUDFOV;
float fAspectRatioScale;
float fNewCamera1FOV;
float fNewCamera2FOV;
float fNewCamera3FOV;
float fNewCamera4FOV;
float fNewCamera5FOV;
float fNewCamera6FOV;
float fNewCamera7FOV;
float fNewCamera8FOV;
float fNewCamera9FOV;
float fNewCamera10FOV;
float fNewCamera11FOV;
float fNewCamera12FOV;
float fNewCamera13FOV;
float fNewCamera14FOV;
float fNewCamera15FOV;
float fNewCamera16FOV;
float fNewCamera17FOV;
float fNewCamera18FOV;
float fNewCamera19FOV;
float fNewCamera20FOV;
float fNewCamera21FOV;

// Game detection
enum class Game
{
	PH2CONFIG,
	PH2,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PH2CONFIG, {"Petz Horsez 2 - Configuration", "HorsezOption.exe"}},
	{Game::PH2, {"Petz Horsez 2", "Jade.exe"}},
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

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * atanf(tanf(fCurrentFOV / 2.0f) * fAspectRatioScale);
}

static SafetyHookMid Camera1FOVInstructionHook{};

void Camera1FOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCamera1FOV = *reinterpret_cast<float*>(ctx.edx + 0x00CF55E0);

	fNewCamera1FOV = CalculateNewFOV(fCurrentCamera1FOV);

	_asm
	{
		fld dword ptr ds:[fNewCamera1FOV]
	}
}

static SafetyHookMid Camera2FOVInstructionHook{};

void Camera2FOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCamera2FOV = *reinterpret_cast<float*>(ctx.eax + 0xC84);

	fNewCamera2FOV = CalculateNewFOV(fCurrentCamera2FOV) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCamera2FOV]
	}
}

static SafetyHookMid Camera8FOVInstructionHook{};

void Camera8FOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCamera8FOV = *reinterpret_cast<float*>(ctx.edx + ctx.ecx * 0x4 + 0x258);

	fNewCamera8FOV = CalculateNewFOV(fCurrentCamera8FOV) * fFOVFactor;

	_asm
	{
		fmul dword ptr ds:[fNewCamera8FOV]
	}
}

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::PH2CONFIG)
		{
			std::vector<const char*> resolutionPatterns =
			{
				// 1680x1050
				"C4 90 06 00 00 75 24 81 7D B8 1A 04 00 00 75 1B 8B F4",
				"88 90 06 00 00 75 2F 81 BD 7C FF FF FF 1A 04 00 00 75 23 8B",
				"C4 90 06 00 00 0F 85 95 01 00 00 81 7D B8 1A 04 00 00 0F 85 88",
			};

			auto resolutionPatternsResult = Memory::MultiPatternScan(exeModule, resolutionPatterns);

			bool bAllFound = std::all_of(resolutionPatternsResult.begin(), resolutionPatternsResult.end(), [](const Memory::PatternInfo& info)
			{
				return info.address != nullptr;
			});

			if (bAllFound == true)
			{
				spdlog::info("Resolution 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[0].address - (std::uint8_t*)exeModule);

				spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[1].address - (std::uint8_t*)exeModule);

				spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), resolutionPatternsResult[2].address - (std::uint8_t*)exeModule);

				Memory::Write(resolutionPatternsResult[0].address + 1, iCurrentResX);

				Memory::Write(resolutionPatternsResult[0].address + 10, iCurrentResY);

				Memory::Write(resolutionPatternsResult[1].address + 1, iCurrentResX);

				Memory::Write(resolutionPatternsResult[1].address + 13, iCurrentResY);

				Memory::Write(resolutionPatternsResult[2].address + 1, iCurrentResX);

				Memory::Write(resolutionPatternsResult[2].address + 14, iCurrentResY);
			}
			else
			{
				spdlog::info("Cannot locate the resolution lists scan memory addresses.");
				return;
			}
		}
		
		if (eGameType == Game::PH2)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::uint8_t* Camera1FOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 82 E0 55 CF 00 5D C3 CC CC");
			if (Camera1FOVInstructionScanResult)
			{
				spdlog::info("Camera 1 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), Camera1FOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(Camera1FOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				Camera1FOVInstructionHook = safetyhook::create_mid(Camera1FOVInstructionScanResult, Camera1FOVInstructionMidHook);
			}
			else
			{
				spdlog::info("Cannot locate the camera 1 FOV instruction memory address.");
				return;
			}

			std::uint8_t* Camera2FOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 80 84 0C 00 00 D8 81 BC 0C 00 00 51 D9 1C 24");
			if (Camera2FOVInstructionScanResult)
			{
				spdlog::info("Camera 2 FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), Camera2FOVInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(Camera2FOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				Camera2FOVInstructionHook = safetyhook::create_mid(Camera2FOVInstructionScanResult, Camera2FOVInstructionMidHook);
			}
			else
			{
				spdlog::info("Cannot locate the camera 2 FOV instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV3InstructionScanResult = Memory::PatternScan(exeModule, "8B 91 D8 0E 00 00 89 55 98 E9 FD 02 00 00 8B 85 7C FF FF FF");
			if (CameraFOV3InstructionScanResult)
			{
				spdlog::info("Camera FOV 3 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV3InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV3InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV3InstructionMidHook{};

				CameraFOV3InstructionMidHook = safetyhook::create_mid(CameraFOV3InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera3FOV = *reinterpret_cast<float*>(ctx.ecx + 0xED8);

					fNewCamera3FOV = CalculateNewFOV(fCurrentCamera3FOV);

					ctx.edx = std::bit_cast<uintptr_t>(fNewCamera3FOV);
  				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 3 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV4InstructionScanResult = Memory::PatternScan(exeModule, "8B 88 F4 0E 00 00 89 4D 98 E9 FD 02 00 00 8B 95 7C FF FF FF");
			if (CameraFOV4InstructionScanResult)
			{
				spdlog::info("Camera FOV 4 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV4InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV4InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV4InstructionMidHook{};

				CameraFOV4InstructionMidHook = safetyhook::create_mid(CameraFOV4InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera4FOV = *reinterpret_cast<float*>(ctx.eax + 0xEF4);

					fNewCamera4FOV = CalculateNewFOV(fCurrentCamera4FOV);

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCamera4FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 4 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV5InstructionScanResult = Memory::PatternScan(exeModule, "89 91 94 00 00 00 8B 45 08 50 8B 4D 08 FF 91 A8 00 00 00 83 C4 04 8B 55 08 8B 82 90 00 00 00 89 45 FC");
			if (CameraFOV5InstructionScanResult)
			{
				spdlog::info("Camera FOV 5 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV5InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV5InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV5InstructionMidHook{};

				CameraFOV5InstructionMidHook = safetyhook::create_mid(CameraFOV5InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera5FOV = std::bit_cast<float>(ctx.edx);

					if (fCurrentCamera5FOV == 1.0f || fCurrentCamera5FOV == 1.200000048f)
					{
						fNewCamera5FOV = CalculateNewFOV(fCurrentCamera5FOV);
					}
					else
					{
						fNewCamera5FOV = fCurrentCamera5FOV;
					}

					*reinterpret_cast<float*>(ctx.ecx + 0x94) = fNewCamera5FOV;
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 5 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV6InstructionScanResult = Memory::PatternScan(exeModule, "8B 91 10 01 00 00 52 8B 45 F8 8B 88 0C 01 00 00 51 8B 55 F8 8B 82 08 01 00 00 50 8B 4D F8");
			if (CameraFOV6InstructionScanResult)
			{
				spdlog::info("Camera FOV 6 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV6InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV6InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV6InstructionMidHook{};

				CameraFOV6InstructionMidHook = safetyhook::create_mid(CameraFOV6InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera6FOV = *reinterpret_cast<float*>(ctx.ecx + 0x110);

					fNewCamera6FOV = CalculateNewFOV(fCurrentCamera6FOV);

					ctx.edx = std::bit_cast<uintptr_t>(fNewCamera6FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 6 instruction memory address.");
				return;
			}

			/*
			std::uint8_t* CameraFOV7InstructionScanResult = Memory::PatternScan(exeModule, "89 94 81 58 02 00 00 8B 45 F8 C7 40 0C 00 00 00 00 83 7D F0 00 75 6F DB 45 F4");
			if (CameraFOV7InstructionScanResult)
			{
				spdlog::info("Camera FOV 7 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV7InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV7InstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90", 7);

				static SafetyHookMid CameraFOV7InstructionMidHook{};

				CameraFOV7InstructionMidHook = safetyhook::create_mid(CameraFOV7InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera7FOV = std::bit_cast<float>(ctx.edx);

					if (fCurrentCamera7FOV == 1.0f || fCurrentCamera7FOV == 1.200000048f)
					{
						fNewCamera7FOV = CalculateNewFOV(fCurrentCamera7FOV);
					}
					else
					{
						fNewCamera7FOV = fCurrentCamera7FOV;
					}

					*reinterpret_cast<float*>(ctx.ecx + ctx.eax * 0x4 + 0x258) = fNewCamera7FOV;
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 6 instruction memory address.");
				return;
			}
			*/

			std::uint8_t* CameraFOV8InstructionScanResult = Memory::PatternScan(exeModule, "D8 8C 8A 58 02 00 00 8B 45 F8 D8 40 0C 8B 4D F8 D9 59 0C 83 7D E4 00");
			if (CameraFOV8InstructionScanResult)
			{
				spdlog::info("Camera FOV 8 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV8InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV8InstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90", 7);

				Camera8FOVInstructionHook = safetyhook::create_mid(CameraFOV8InstructionScanResult, Camera8FOVInstructionMidHook);
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 8 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV9InstructionScanResult = Memory::PatternScan(exeModule, "8B 88 48 0F 00 00 89 4D 98 E9 62 02 00 00 8B 95 7C FF FF FF 81 C2 74 0F 00 00 8B 02 89 45 9C 8B 4A 04");
			if (CameraFOV9InstructionScanResult)
			{
				spdlog::info("Camera FOV 9 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV9InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV9InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV9InstructionMidHook{};

				CameraFOV9InstructionMidHook = safetyhook::create_mid(CameraFOV9InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera9FOV = *reinterpret_cast<float*>(ctx.eax + 0xF48);

					fNewCamera9FOV = CalculateNewFOV(fCurrentCamera9FOV);

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCamera9FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 9 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV10InstructionScanResult = Memory::PatternScan(exeModule, "8B 88 1C 0E 00 00 51 8B 55 FC 8B 82 20 0E 00 00 50 E8 D2 5F F4 FF 83 C4 08 D9 1C 24");
			if (CameraFOV10InstructionScanResult)
			{
				spdlog::info("Camera FOV 10 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV10InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV10InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV10InstructionMidHook{};

				CameraFOV10InstructionMidHook = safetyhook::create_mid(CameraFOV10InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera10FOV = *reinterpret_cast<float*>(ctx.eax + 0xE1C);

					if (fCurrentCamera10FOV == 1.100000024f || fCurrentCamera10FOV == 0.6000000238f)
					{
						fNewCamera10FOV = CalculateNewFOV(fCurrentCamera10FOV);
					}
					else
					{
						fNewCamera10FOV = fCurrentCamera10FOV;
					}

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCamera10FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 10 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV11InstructionScanResult = Memory::PatternScan(exeModule, "8B 88 38 12 00 00 89 4D 98 E9 E0 00 00 00 8B 95 7C FF FF FF 81 C2 64 12 00 00 8B 02 89 45 9C 8B 4A 04 89 4D A0");
			if (CameraFOV11InstructionScanResult)
			{
				spdlog::info("Camera FOV 11 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV11InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV11InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV11InstructionMidHook{};

				CameraFOV11InstructionMidHook = safetyhook::create_mid(CameraFOV11InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera11FOV = *reinterpret_cast<float*>(ctx.eax + 0x1238);

					fNewCamera11FOV = CalculateNewFOV(fCurrentCamera11FOV);

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCamera11FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 11 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV12InstructionScanResult = Memory::PatternScan(exeModule, "8B 82 10 0F 00 00 89 45 98 E9 B0 02 00 00 8B 8D 7C FF FF FF 81 C1 3C 0F 00 00 8B 11 89 55 9C 8B 41 04 89 45 A0 8B 49 08");
			if (CameraFOV12InstructionScanResult)
			{
				spdlog::info("Camera FOV 12 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV12InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV12InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV12InstructionMidHook{};

				CameraFOV12InstructionMidHook = safetyhook::create_mid(CameraFOV12InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera12FOV = *reinterpret_cast<float*>(ctx.edx + 0xF10);

					fNewCamera12FOV = CalculateNewFOV(fCurrentCamera12FOV);

					ctx.eax = std::bit_cast<uintptr_t>(fNewCamera12FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 12 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV13InstructionScanResult = Memory::PatternScan(exeModule, "8B 82 74 11 00 00 89 45 98 E9 16 02 00 00 8B 8D 7C FF FF FF 81 C1 A0 11 00 00 8B 11 89 55 9C 8B 41 04 89 45 A0 8B 49 08 89 4D A4");
			if (CameraFOV13InstructionScanResult)
			{
				spdlog::info("Camera FOV 13 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV13InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV13InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV13InstructionMidHook{};

				CameraFOV13InstructionMidHook = safetyhook::create_mid(CameraFOV13InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera13FOV = *reinterpret_cast<float*>(ctx.edx + 0x1174);

					fNewCamera13FOV = CalculateNewFOV(fCurrentCamera13FOV);

					ctx.eax = std::bit_cast<uintptr_t>(fNewCamera13FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 13 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV14InstructionScanResult = Memory::PatternScan(exeModule, "8B 91 08 05 00 00 52 6A 00 E8 C6 F1 F5 FF 83 C4 08 8B 45 A4 05 98 00 00 00 8B 8D 74 FF FF FF 89 08 8B 95 78 FF FF FF 89 50 04 8B 8D 7C FF FF FF");
			if (CameraFOV14InstructionScanResult)
			{
				spdlog::info("Camera FOV 14 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV14InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV14InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV14InstructionMidHook{};

				CameraFOV14InstructionMidHook = safetyhook::create_mid(CameraFOV14InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera14FOV = *reinterpret_cast<float*>(ctx.ecx + 0x508);

					fNewCamera14FOV = CalculateNewFOV(fCurrentCamera14FOV);

					ctx.edx = std::bit_cast<uintptr_t>(fNewCamera14FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 14 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV15InstructionScanResult = Memory::PatternScan(exeModule, "8B 88 90 11 00 00 89 4D 98 E9 C8 01 00 00 8B 95 7C FF FF FF 81 C2 BC 11 00 00 8B 02 89 45 9C 8B 4A 04 89 4D A0 8B 52 08 89 55 A4");
			if (CameraFOV15InstructionScanResult)
			{
				spdlog::info("Camera FOV 15 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV15InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV15InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV15InstructionMidHook{};

				CameraFOV15InstructionMidHook = safetyhook::create_mid(CameraFOV15InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera15FOV = *reinterpret_cast<float*>(ctx.eax + 0x1190);

					fNewCamera15FOV = CalculateNewFOV(fCurrentCamera15FOV);

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCamera15FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 15 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV16InstructionScanResult = Memory::PatternScan(exeModule, "8B 88 AC 11 00 00 89 4D 98 E9 C8 01 00 00 8B 95 7C FF FF FF 81 C2 D8 11 00 00 8B 02 89 45 9C 8B 4A 04 89 4D A0 8B 52 08 89 55 A4");
			if (CameraFOV16InstructionScanResult)
			{
				spdlog::info("Camera FOV 16 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV16InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV16InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV16InstructionMidHook{};

				CameraFOV16InstructionMidHook = safetyhook::create_mid(CameraFOV16InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera16FOV = *reinterpret_cast<float*>(ctx.eax + 0x11AC);

					fNewCamera16FOV = CalculateNewFOV(fCurrentCamera16FOV);

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCamera16FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 16 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV17InstructionScanResult = Memory::PatternScan(exeModule, "8B 91 70 12 00 00 89 55 98 E9 93 00 00 00 8B 85 7C FF FF FF 05 9C 12 00 00 8B 08 89 4D 9C 8B 50 04 89 55 A0 8B 40 08");
			if (CameraFOV17InstructionScanResult)
			{
				spdlog::info("Camera FOV 17 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV17InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV17InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV17InstructionMidHook{};

				CameraFOV17InstructionMidHook = safetyhook::create_mid(CameraFOV17InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera17FOV = *reinterpret_cast<float*>(ctx.ecx + 0x1270);

					fNewCamera17FOV = CalculateNewFOV(fCurrentCamera17FOV);

					ctx.edx = std::bit_cast<uintptr_t>(fNewCamera17FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 17 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV18InstructionScanResult = Memory::PatternScan(exeModule, "8B 88 98 10 00 00 89 4D 98 E9 92 00 00 00 8B 95 7C FF FF FF 81 C2 C4 10 00 00 8B 02 89 45 9C 8B 4A 04 89 4D A0 8B 52 08 89 55 A4");
			if (CameraFOV18InstructionScanResult)
			{
				spdlog::info("Camera FOV 18 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV18InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV18InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV18InstructionMidHook{};

				CameraFOV18InstructionMidHook = safetyhook::create_mid(CameraFOV18InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera18FOV = *reinterpret_cast<float*>(ctx.eax + 0x1098);

					fNewCamera18FOV = CalculateNewFOV(fCurrentCamera18FOV);

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCamera18FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 18 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV19InstructionScanResult = Memory::PatternScan(exeModule, "8B 82 B4 10 00 00 89 45 98 E9 93 00 00 00 8B 8D 7C FF FF FF 81 C1 E0 10 00 00 8B 11 89 55 9C 8B 41 04 89 45 A0 8B 49 08 89 4D A4");
			if (CameraFOV19InstructionScanResult)
			{
				spdlog::info("Camera FOV 19 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV19InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV19InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV19InstructionMidHook{};

				CameraFOV19InstructionMidHook = safetyhook::create_mid(CameraFOV19InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera19FOV = *reinterpret_cast<float*>(ctx.edx + 0x10B4);

					fNewCamera19FOV = CalculateNewFOV(fCurrentCamera19FOV);

					ctx.eax = std::bit_cast<uintptr_t>(fNewCamera19FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 19 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV20InstructionScanResult = Memory::PatternScan(exeModule, "8B 91 80 0F 00 00 89 55 98 E9 15 02 00 00 8B 85 7C FF FF FF 05 AC 0F 00 00 8B 08 89 4D 9C 8B 50 04 89 55 A0 8B 40 08 89 45 A4");
			if (CameraFOV20InstructionScanResult)
			{
				spdlog::info("Camera FOV 20 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV20InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV20InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV20InstructionMidHook{};

				CameraFOV20InstructionMidHook = safetyhook::create_mid(CameraFOV20InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera20FOV = *reinterpret_cast<float*>(ctx.ecx + 0xF80);

					fNewCamera20FOV = CalculateNewFOV(fCurrentCamera20FOV);

					ctx.edx = std::bit_cast<uintptr_t>(fNewCamera20FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 20 instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOV21InstructionScanResult = Memory::PatternScan(exeModule, "8B 82 84 0C 00 00 89 81 1C 0E 00 00 EB 12 8B 4D F0 8B 55 F0 8B 82 2C 05 00 00 89 81 1C 0E 00 00 6A 00");
			if (CameraFOV21InstructionScanResult)
			{
				spdlog::info("Camera FOV 21 Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOV21InstructionScanResult - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOV21InstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

				static SafetyHookMid CameraFOV21InstructionMidHook{};

				CameraFOV21InstructionMidHook = safetyhook::create_mid(CameraFOV21InstructionScanResult, [](SafetyHookContext& ctx)
				{
					float fCurrentCamera21FOV = *reinterpret_cast<float*>(ctx.edx + 0xC84);

					fNewCamera21FOV = CalculateNewFOV(fCurrentCamera21FOV);

					ctx.eax = std::bit_cast<uintptr_t>(fNewCamera21FOV);
				});
			}
			else
			{
				spdlog::info("Cannot locate the camera FOV 21 instruction memory address.");
				return;
			}

			std::uint8_t* HUDFOVScanResult = Memory::PatternScan(exeModule, "C7 45 DC 00 00 80 3F C7 45 F4 FF FF FF FF C7 45 F8 FE FE FE FE");
			if (HUDFOVScanResult)
			{
				spdlog::info("HUD FOV Scan: Address is {:s}+{:x}", sExeName.c_str(), HUDFOVScanResult - (std::uint8_t*)exeModule);

				fNewHUDFOV = CalculateNewFOV(1.0f);
				
				Memory::Write(HUDFOVScanResult + 3, fNewHUDFOV);
			}
			else
			{
				spdlog::info("Cannot locate the HUD FOV scan memory address.");
				return;
			}
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