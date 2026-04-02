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
std::string sFixName = "ProjectIGI1WidescreenFix";
std::string sFixVersion = "1.6";
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
constexpr float fOriginalAspectRatio = 4.0f;
constexpr double dOriginalHipfireFOV = 0.785398185253143;
constexpr double dOriginalWeaponFOV = 0.523598790168762;

// Ini variables
bool bFixActive;
uint32_t iNewResX;
uint32_t iNewResY;
double dCameraFOVFactor;
double dWeaponFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
float fNewGeneralFOV;
double dNewHipfireFOV;
double dNewWeaponFOV;
uint8_t iInsideComputer;

// Game detection
enum class Game
{
	PI1,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	General,
	Hipfire1,
	Hipfire2,
	Hipfire3,
	Hipfire4,
	Hipfire5,
	Hipfire6,
	Hipfire7,
	Hipfire8,
	Hipfire9,
	Hipfire10,
	Hipfire11,
	Hipfire12,
	Hipfire13,
	Weapon1,
	Weapon2,
	Weapon3,
	Weapon4,
	Weapon5,
	Weapon6
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PI1, {"Project IGI 1: I'm Going In", "IGI.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "Width", iNewResX);
	inipp::get_value(ini.sections["Settings"], "Height", iNewResY);
	inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", dCameraFOVFactor);
	inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", dWeaponFOVFactor);
	spdlog_confparse(iNewResX);
	spdlog_confparse(iNewResY);
	spdlog_confparse(dCameraFOVFactor);
	spdlog_confparse(dWeaponFOVFactor);

	// If resolution not specified, use desktop resolution
	if (iNewResX <= 0 || iNewResY <= 0)
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();
		iNewResX = desktopDimensions.first;
		iNewResY = desktopDimensions.second;
		spdlog_confparse(iNewResX);
		spdlog_confparse(iNewResY);
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

static SafetyHookMid ResolutionInstructionsHook{};
static SafetyHookMid GeneralFOVInstructionHook{};
static SafetyHookMid InsideComputerInstructionHook{};
static SafetyHookMid BriefingMapFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::PI1 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iNewResX) / static_cast<float>(iNewResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionScanResult = Memory::PatternScan(exeModule, "8B 45 ?? 89 44 24 ?? 8B 4D");
		if (ResolutionInstructionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionScanResult - (std::uint8_t*)exeModule);			

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ebp + 0xC) = iNewResX;

				*reinterpret_cast<int*>(ctx.ebp + 0x10) = iNewResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? DB 44 24 ?? D8 0D");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			fNewAspectRatio2 = fOriginalAspectRatio * fAspectRatioScale;

			Memory::Write(AspectRatioInstructionScanResult + 2, &fNewAspectRatio2);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule,
		/*General*/
		"D9 40 ?? D8 4B ?? D9 1C 24",
		/*Hipfire*/
		"DD 05 ?? ?? ?? ?? D9 F2 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8D 54 24", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D8 9D",
		"DD 05 ?? ?? ?? ?? D9 F2 83 C4 ?? 89 9E", "DD 05 ?? ?? ?? ?? D9 F2 53", "DD 05 ?? ?? ?? ?? D9 F2 8B 44 24 ?? C7 80", "DD 05 ?? ?? ?? ?? D9 F2 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8D 44 24",
		"DD 05 ?? ?? ?? ?? D9 F2 8B 4C 24", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 5C 24 ?? D8 54 24", "DD 05 ?? ?? ?? ?? D9 F2 8B 44 24 ?? DD D8", "DD 05 ?? ?? ?? ?? D9 F2 8B 44 24 ?? 33 C9", "DD 05 ?? ?? ?? ?? D9 F2 83 C4 ?? DD D8",
		"DD 05 ?? ?? ?? ?? D9 F2 B9 ?? ?? ?? ?? 8D B4 24 ?? ?? ?? ?? 8D BC 24 ?? ?? ?? ?? 83 C4 ?? 89 AC 24", "DD 05 ?? ?? ?? ?? D9 F2 B9 ?? ?? ?? ?? 8D B4 24 ?? ?? ?? ?? 8D BC 24 ?? ?? ?? ?? 83 C4 ?? F3 A5",
		/*Weapon*/
		"DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 9E", "DD 05 ?? ?? ?? ?? D9 F2 8B 54 24", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 98 ?? ?? ?? ?? C3", "DD 05 ?? ?? ?? ?? D9 F2 5F", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 5C 24 ?? 8B 44 24",
		"DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 98 ?? ?? ?? ?? 89 97");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[General] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire1] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire2] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire3] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire4] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire5] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire6] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 7: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire7] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 8: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire8] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 9: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire9] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 10: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire10] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 11: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire11] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 12: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire12] - (std::uint8_t*)exeModule);
			spdlog::info("Hipfire FOV Instruction 13: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Hipfire13] - (std::uint8_t*)exeModule);
			spdlog::info("Weapon FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Weapon1] - (std::uint8_t*)exeModule);
			spdlog::info("Weapon FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Weapon2] - (std::uint8_t*)exeModule);
			spdlog::info("Weapon FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Weapon3] - (std::uint8_t*)exeModule);
			spdlog::info("Weapon FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Weapon4] - (std::uint8_t*)exeModule);
			spdlog::info("Weapon FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Weapon5] - (std::uint8_t*)exeModule);
			spdlog::info("Weapon FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[Weapon6] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[General], 3);

			GeneralFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[General], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = Memory::ReadMem(ctx.eax + 0x40);				

				if (iInsideComputer == 0)
				{
					fNewGeneralFOV = fCurrentGeneralFOV * fAspectRatioScale;
				}
				else if (iInsideComputer == 1)
				{
					fNewGeneralFOV = (fCurrentGeneralFOV * fAspectRatioScale) / (float)dCameraFOVFactor;
				}

				FPU::FLD(fNewGeneralFOV);
			});

			dNewHipfireFOV = dOriginalHipfireFOV * dCameraFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult, Hipfire1, Hipfire13, 2, &dNewHipfireFOV);

			dNewWeaponFOV = Maths::CalculateNewFOV_RadBased(dOriginalWeaponFOV, fAspectRatioScale, Maths::AngleMode::HalfAngle) * dWeaponFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult, Weapon1, Weapon6, 2, &dNewWeaponFOV);
		}

		std::uint8_t* InsideComputerInstructionScanResult = Memory::PatternScan(exeModule, "A2 ?? ?? ?? ?? D9 5C 24 10 F3 A5 D9 E0 D9 44 24 0C D9 E0 D9 44 24 10 D9 E0 D9 44 24 1C D8 C9 D9 44 24 18 D8 CB DE C1 D9 44 24 14 D8 CC DE C1 D9 5C 24 08 D9 44 24 28 D8 C9 D9 44 24 24 D8 CB DE C1 D9 44 24 20 D8 CC DE C1 D9 5C 24 0C D9 44 24 34 D8 C9 D9 44 24 30 D8 CB DE C1 D9 44 24 2C D8 CC DE C1 D9 5C 24 10 8B 44 24 08 B9 0A 00 00 00 8D 74 24 14");
		if (InsideComputerInstructionScanResult)
		{
			spdlog::info("Inside Computer Instruction: Address is{:s} + {:x}", sExeName.c_str(), InsideComputerInstructionScanResult - (std::uint8_t*)exeModule);

			InsideComputerInstructionHook = safetyhook::create_mid(InsideComputerInstructionScanResult, [](SafetyHookContext& ctx)
			{
				iInsideComputer = static_cast<uint8_t>(ctx.eax & 0xFF);		
			});
		}
		else
		{
			spdlog::error("Failed to locate inside computer instruction memory address.");
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
