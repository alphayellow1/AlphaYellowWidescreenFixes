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
std::string sFixName = "ChampionshipSnowboarding2004WidescreenFix";
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
float fAspectRatioScale;
float fNewAspectRatio;
float fNewGroundCameraFOV;
float fNewOverviewCameraFOV1;
float fNewOverviewCameraFOV2;
float fNewOverviewCameraFOV3;
float fNewOverviewCameraFOV4;
float fNewOverviewCameraFOV5;
float fNewOverviewCameraFOV6;
float fNewOverviewCameraFOV7;
float fNewOverviewCameraFOV8;
float fCurrentGroundCameraFOV;

// Game detection
enum class Game
{
	CS2004,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CS2004, {"Championship Snowboarding 2004", "SnowBoard_Core.exe"}},
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

static SafetyHookMid GroundCameraFOVInstructionHook{};
static SafetyHookMid OverviewCameraFOVInstruction1Hook{};
static SafetyHookMid OverviewCameraFOVInstruction2Hook{};
static SafetyHookMid OverviewCameraFOVInstruction3Hook{};
static SafetyHookMid OverviewCameraFOVInstruction4Hook{};
static SafetyHookMid OverviewCameraFOVInstruction5Hook{};
static SafetyHookMid OverviewCameraFOVInstruction6Hook{};
static SafetyHookMid OverviewCameraFOVInstruction7Hook{};
static SafetyHookMid OverviewCameraFOVInstruction8Hook{};

void WidescreenFix()
{
	if (eGameType == Game::CS2004 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* MainMenuResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "C7 44 24 34 20 03 00 00 C7 44 24 38 58 02 00 00");
		if (MainMenuResolutionInstructionsScanResult)
		{
			spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), MainMenuResolutionInstructionsScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(MainMenuResolutionInstructionsScanResult + 4, iCurrentResX);
			
			Memory::Write(MainMenuResolutionInstructionsScanResult + 12, iCurrentResY);

			int iNewBitDepth = 32;

			Memory::Write(MainMenuResolutionInstructionsScanResult + 20, iNewBitDepth);
		}
		else
		{
			spdlog::error("Failed to locate main menu resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 40 2C 00 04 00 00 E8 60 2D 06 00 C7 40 30 00 03 00 00 C2 04 00 E8 51 2D 06 00 C7 40 2C 20 03 00 00 E8 45 2D 06 00 C7 40 30 58 02 00 00 C2 04 00 E8 36 2D 06 00 C7 40 2C 00 05 00 00 E8 2A 2D 06 00 C7 40 30 C0 03 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 3, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 15, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 30, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 42, iCurrentResY);

			// 1280x960
			Memory::Write(ResolutionListScanResult + 57, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 69, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		std::uint8_t* GroundCameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 04 8B 44 24 04 D8 0D AC 05 5E 00 89 41 38 D9 C0 D9 F2");
		if (GroundCameraFOVInstructionScanResult)
		{
			spdlog::info("Ground Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), GroundCameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(GroundCameraFOVInstructionScanResult, 4);

			GroundCameraFOVInstructionHook = safetyhook::create_mid(GroundCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				fCurrentGroundCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x4);

				fNewGroundCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentGroundCameraFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewGroundCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate ground camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D8 72 44 D9 95 0B 03 00 00 D9 9D 13 03 00 00 D9 85 0B 03 00 00 8B 8D 45 04 00 00 DC C0 53 52 D9 9D 1B 03 00 00");
		if (OverviewCameraFOVInstruction1ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(OverviewCameraFOVInstruction1ScanResult, 3);

			OverviewCameraFOVInstruction1Hook = safetyhook::create_mid(OverviewCameraFOVInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV1 = *reinterpret_cast<float*>(ctx.edx + 0x44);

				fNewOverviewCameraFOV1 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV1, fAspectRatioScale) * fFOVFactor;

				FPU::FDIV(fNewOverviewCameraFOV1);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D8 76 44 D9 C0 D8 C1 D9 5C 24 20 8B 4C 24 20 D8 4E 3C 89 4C 24 14 8D 4C 24 14 51 DC C0 53 D9 5C 24 2C D9 46 34");
		if (OverviewCameraFOVInstruction2ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(OverviewCameraFOVInstruction2ScanResult, 3);

			OverviewCameraFOVInstruction2Hook = safetyhook::create_mid(OverviewCameraFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x44);

				if ((fCurrentOverviewCameraFOV2 > 2.23f && fCurrentOverviewCameraFOV2 < 3.22f) || fCurrentOverviewCameraFOV2 == 42.857639312744f || fCurrentOverviewCameraFOV2 == 43.261646270752f || fCurrentOverviewCameraFOV2 == 43.261707305908f || fCurrentOverviewCameraFOV2 == 42.857646942139f || fCurrentOverviewCameraFOV2 == 34.321136474609f)
				{
					fNewOverviewCameraFOV2 = fCurrentOverviewCameraFOV2;
				}
				else
				{
					fNewOverviewCameraFOV2 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV2, fAspectRatioScale) * fFOVFactor;
				}

				FPU::FDIV(fNewOverviewCameraFOV2);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D8 49 44 D9 5C 24 00 D9 41 50 D8 2D E8 05 5E 00 D8 49 44 D9 5C 24 04 D9 41 44");
		if (OverviewCameraFOVInstruction3ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(OverviewCameraFOVInstruction3ScanResult, 3);

			OverviewCameraFOVInstruction3Hook = safetyhook::create_mid(OverviewCameraFOVInstruction3ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV3 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

				fNewOverviewCameraFOV3 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV3, fAspectRatioScale) * fFOVFactor;

				FPU::FMUL(fNewOverviewCameraFOV3);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "D8 49 44 D9 5C 24 04 D9 41 44 D8 0D AC 05 5E 00 D9 5C 24 10 75 0C D9 41 4C D8 49 44 D8 6C 24 10 EB 10 D9 41 54");
		if (OverviewCameraFOVInstruction4ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction4ScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(OverviewCameraFOVInstruction4ScanResult, 3);

			OverviewCameraFOVInstruction4Hook = safetyhook::create_mid(OverviewCameraFOVInstruction4ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV4 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

				fNewOverviewCameraFOV4 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV4, fAspectRatioScale) * fFOVFactor;

				FPU::FMUL(fNewOverviewCameraFOV4);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction5ScanResult = Memory::PatternScan(exeModule, "D8 49 44 D8 6C 24 10 EB 10 D9 41 54 D8 2D E8 05 5E 00 D8 49 44 D8 64 24 10 A8 01 D8 71 3C 75 1E D9 44 24 10 8B 44 24 0C D8 64 24 00");
		if (OverviewCameraFOVInstruction5ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction5ScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(OverviewCameraFOVInstruction5ScanResult, 3);

			OverviewCameraFOVInstruction5Hook = safetyhook::create_mid(OverviewCameraFOVInstruction5ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV5 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

				fNewOverviewCameraFOV5 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV5, fAspectRatioScale) * fFOVFactor;

				FPU::FMUL(fNewOverviewCameraFOV5);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 5 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction6ScanResult = Memory::PatternScan(exeModule, "D9 41 44 D8 0D AC 05 5E 00 D9 5C 24 10 75 0C D9 41 4C");
		if (OverviewCameraFOVInstruction6ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction6ScanResult - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(OverviewCameraFOVInstruction6ScanResult, 3);
			
			OverviewCameraFOVInstruction6Hook = safetyhook::create_mid(OverviewCameraFOVInstruction6ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV6 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

				fNewOverviewCameraFOV6 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV6, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewOverviewCameraFOV6);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 6 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction7ScanResult = Memory::PatternScan(exeModule, "D9 41 44 DC 0D 80 21 5E 00 DE E9 D9 44 24 14 D9 18 D9 58 04 D9 58 08 83 C4 08 C2 0C 00 A9 FE FF FF FF D8 4C 24 14 D8 49 58 DC C0");
		if (OverviewCameraFOVInstruction7ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction7ScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(OverviewCameraFOVInstruction7ScanResult, 3);

			OverviewCameraFOVInstruction7Hook = safetyhook::create_mid(OverviewCameraFOVInstruction7ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV7 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

				fNewOverviewCameraFOV7 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV7, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewOverviewCameraFOV7);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 7 memory address.");
			return;
		}

		std::uint8_t* OverviewCameraFOVInstruction8ScanResult = Memory::PatternScan(exeModule, "D8 49 44 D8 64 24 10 A8 01 D8 71 3C 75 1E D9 44 24 10 8B 44 24 0C D8 64 24 00 D9 44 24 14 D9 18 D9 58 04 D9 58 08 83 C4 08");
		if (OverviewCameraFOVInstruction8ScanResult)
		{
			spdlog::info("Overview Camera FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), OverviewCameraFOVInstruction8ScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(OverviewCameraFOVInstruction8ScanResult, 3);

			OverviewCameraFOVInstruction8Hook = safetyhook::create_mid(OverviewCameraFOVInstruction8ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentOverviewCameraFOV8 = *reinterpret_cast<float*>(ctx.ecx + 0x44);

				fNewOverviewCameraFOV8 = Maths::CalculateNewFOV_MultiplierBased(fCurrentOverviewCameraFOV8, fAspectRatioScale) * fFOVFactor;

				FPU::FMUL(fNewOverviewCameraFOV8);
			});
		}
		else
		{
			spdlog::error("Failed to locate overview camera FOV instruction 8 memory address.");
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