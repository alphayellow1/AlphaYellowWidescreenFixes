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
#include <vector>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule2 = nullptr;
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "Cabelas4x4OffroadAdventure3FOVFix";
std::string sFixVersion = "1.3";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fWeaponZoomFactor;
float fBinocularsZoomFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewAspectRatio2;
float fNewGeneralFOV;
float fNewWeaponZoomFOV;
float fNewBinocularsZoomFOV;
uint32_t iInsideCar;
uint8_t* InsideCarTriggerValueAddress;

// Game detection
enum class Game
{
	C4X4OA3,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	GeneralFOV,
	WeaponZoomFOV1,
	WeaponZoomFOV2,
	WeaponZoomFOV3,
	BinocularsZoomFOV1,
	BinocularsZoomFOV2
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::C4X4OA3, {"Cabela's 4x4 Offroad Adventure 3", "Engine.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "WeaponZoomFactor", fWeaponZoomFactor);
	inipp::get_value(ini.sections["Settings"], "BinocularsZoomFactor", fBinocularsZoomFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fWeaponZoomFactor);
	spdlog_confparse(fBinocularsZoomFactor);

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

	dllModule2 = Memory::GetHandle("EngineDll.dll");

	dllModule3 = Memory::GetHandle("GameDll.dll");

	return true;
}

static SafetyHookMid InsideCarTriggerInstructionHook{};
static SafetyHookMid AspectRatioInstructionsHook{};
static SafetyHookMid GeneralFOVInstructionHook{};
static SafetyHookMid WeaponZoomFOVInstruction1Hook{};

void FOVFix()
{
	if (eGameType == Game::C4X4OA3 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* InsideCarTriggerInstructionScanResult = Memory::PatternScan(exeModule, "39 1D ?? ?? ?? ?? 75 ?? 8B 8E");
		if (InsideCarTriggerInstructionScanResult)
		{
			spdlog::info("Inside Car Trigger Instruction: Address is {:s}+{:x}", sExeName.c_str(), InsideCarTriggerInstructionScanResult - (std::uint8_t*)exeModule);

			InsideCarTriggerValueAddress = Memory::GetPointerFromAddress<uint32_t>(InsideCarTriggerInstructionScanResult + 2, Memory::PointerMode::Absolute);			

			InsideCarTriggerInstructionHook = safetyhook::create_mid(InsideCarTriggerInstructionScanResult, [](SafetyHookContext& ctx)
			{
				iInsideCar = *reinterpret_cast<uint32_t*>(InsideCarTriggerValueAddress);
			});
		}
		else
		{
			spdlog::error("Failed to locate inside car trigger instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionsScanResult = Memory::PatternScan(dllModule2, "D9 83 ?? ?? ?? ?? D9 83 ?? ?? ?? ?? D9 C2");
		if (AspectRatioInstructionsScanResult)
		{
			spdlog::info("Aspect Ratio Instructions Scan: Address is EngineDll.dll+{:x}", AspectRatioInstructionsScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(AspectRatioInstructionsScanResult, 12);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			AspectRatioInstructionsHook = safetyhook::create_mid(AspectRatioInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD(fNewAspectRatio2);

				FPU::FLD(1.0f);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 83 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2", dllModule3, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 01 74 0B",
		"C7 41 ?? ?? ?? ?? ?? C6 41", "C7 46 ?? ?? ?? ?? ?? EB ?? 88 5E", "D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 74 ?? C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? D9 41", "C7 41 ?? ?? ?? ?? ?? C7 41 ?? ?? ?? ?? ?? D9 41");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is EngineDll.dll+{:x}", CameraFOVInstructionsScansResult[GeneralFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Weapon Zoom FOV Instruction 1: Address is GameDll.dll+{:x}", CameraFOVInstructionsScansResult[WeaponZoomFOV1] - (std::uint8_t*)dllModule3);

			spdlog::info("Weapon Zoom FOV Instruction 2: Address is GameDll.dll+{:x}", CameraFOVInstructionsScansResult[WeaponZoomFOV2] - (std::uint8_t*)dllModule3);

			spdlog::info("Weapon Zoom FOV Instruction 3: Address is GameDll.dll+{:x}", CameraFOVInstructionsScansResult[WeaponZoomFOV3] - (std::uint8_t*)dllModule3);

			spdlog::info("Binoculars Zoom FOV Instruction 1: Address is GameDll.dll+{:x}", CameraFOVInstructionsScansResult[BinocularsZoomFOV1] - (std::uint8_t*)dllModule3);

			spdlog::info("Binoculars Zoom FOV Instruction 2: Address is GameDll.dll+{:x}", CameraFOVInstructionsScansResult[BinocularsZoomFOV2] - (std::uint8_t*)dllModule3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GeneralFOV], 6);

			GeneralFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GeneralFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = *reinterpret_cast<float*>(ctx.ebx + 0xD0);

				if (iInsideCar == 0)
				{
					if (fCurrentGeneralFOV == 75.0f)
					{
						fNewGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, fAspectRatioScale) * fFOVFactor;
					}
					else
					{
						fNewGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, fAspectRatioScale);
					}
				}
				else if (iInsideCar == 1)
				{
					fNewGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, fAspectRatioScale);
				}

				FPU::FLD(fNewGeneralFOV);
			});

			fNewWeaponZoomFOV = 70.0f / fWeaponZoomFactor;

			fNewBinocularsZoomFOV = 5.0f / fBinocularsZoomFactor;

			Memory::Write(CameraFOVInstructionsScansResult[WeaponZoomFOV1] + 2, &fNewWeaponZoomFOV);

			Memory::Write(CameraFOVInstructionsScansResult[WeaponZoomFOV2] + 3, fNewWeaponZoomFOV);

			Memory::Write(CameraFOVInstructionsScansResult[WeaponZoomFOV3] + 3, fNewWeaponZoomFOV);

			Memory::Write(CameraFOVInstructionsScansResult[BinocularsZoomFOV1] + 2, &fNewBinocularsZoomFOV);

			Memory::Write(CameraFOVInstructionsScansResult[BinocularsZoomFOV2] + 3, fNewBinocularsZoomFOV);
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