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
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "RevolutionFOVFix";
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
float fNewAspectRatio2;
float fNewGeneralFOV;
float fNewCutscenesFOV1;
float fNewCutscenesFOV2;
float fNewInterfaceWeaponFOV;

// Game detection
enum class Game
{
	REV,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	GeneralFOV,
	CutscenesFOV1,
	CutscenesFOV2,
	InterfaceWeaponFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::REV, {"Revolution", "MainDll.dll"}},
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

	dllModule3 = Memory::GetHandle("InterfaceDll.dll");

	return true;
}

static SafetyHookMid AspectRatioInstructionsHook{};
static SafetyHookMid GeneralFOVInstructionHook{};
static SafetyHookMid CutscenesFOVInstruction1Hook{};
static SafetyHookMid CutscenesFOVInstruction2Hook{};

void FOVFix()
{
	if (eGameType == Game::REV && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionsScanResult = Memory::PatternScan(dllModule2, "d9 83 ?? ?? ?? ?? d9 83 ?? ?? ?? ?? d9 c2");
		if (AspectRatioInstructionsScanResult)
		{
			spdlog::info("Aspect Ratio Instructions Scan: Address is EngineDll.dll+{:x}", AspectRatioInstructionsScanResult - (std::uint8_t*)dllModule2);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			Memory::WriteNOPs(AspectRatioInstructionsScanResult, 12);
			
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

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 83 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2", "D9 45 ?? D8 9B", "8B 45 ?? 8B CB 89 83",
		dllModule3, "68 ?? ?? ?? ?? 8D 8D ?? ?? ?? ?? FF D6");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is EngineDll.dll+{:x}", CameraFOVInstructionsScansResult[GeneralFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Cutscenes FOV Instruction 1: Address is EngineDll.dll+{:x}", CameraFOVInstructionsScansResult[CutscenesFOV1] - (std::uint8_t*)dllModule2);

			spdlog::info("Cutscenes FOV Instruction 2: Address is EngineDll.dll+{:x}", CameraFOVInstructionsScansResult[CutscenesFOV2] - (std::uint8_t*)dllModule2);

			spdlog::info("Interface Weapon FOV Instruction: Address is InterfaceDll.dll+{:x}", CameraFOVInstructionsScansResult[InterfaceWeaponFOV] - (std::uint8_t*)dllModule3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GeneralFOV], 6);

			GeneralFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GeneralFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = *reinterpret_cast<float*>(ctx.ebx + 0xD0);

				fNewGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewGeneralFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV1], 3);

			CutscenesFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV1 = *reinterpret_cast<float*>(ctx.ebp + 0x10);

				fNewCutscenesFOV1 = fCurrentCutscenesFOV1 / fFOVFactor;

				FPU::FLD(fNewCutscenesFOV1);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV2], 3);

			CutscenesFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV2 = *reinterpret_cast<float*>(ctx.ebp + 0x10);

				fNewCutscenesFOV2 = fCurrentCutscenesFOV2 / fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewCutscenesFOV2);
			});

			fNewInterfaceWeaponFOV = 60.0f / fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[InterfaceWeaponFOV] + 1, fNewInterfaceWeaponFOV);
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