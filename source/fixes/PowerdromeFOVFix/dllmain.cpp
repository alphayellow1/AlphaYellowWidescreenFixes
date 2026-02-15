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
#include <cmath>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "PowerdromeFOVFix";
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
uint8_t* GameplayFOVAddress1;
float fNewCameraFOV;

// Game detection
enum class Game
{
	PD,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	MainMenuFOV,
	CutscenesFOV1,
	CutscenesFOV2,
	CutscenesFOV3,
	GameplayFOV1,
	GameplayFOV2,
	GameplayFOV3,
	GameplayFOV4
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PD, {"Powerdrome", "Flux.exe"}},
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

	dllModule2 = Memory::GetHandle("powerdrome.dll");

	return true;
}

static SafetyHookMid MainMenuCameraFOVInstructionHook{};
static SafetyHookMid CutscenesCameraFOVInstruction1Hook{};
static SafetyHookMid CutscenesCameraFOVInstruction2Hook{};
static SafetyHookMid CutscenesCameraFOVInstruction3Hook{};
static SafetyHookMid GameplayCameraFOVInstruction1Hook{};
static SafetyHookMid GameplayCameraFOVInstruction2Hook{};
static SafetyHookMid GameplayCameraFOVInstruction3Hook{};
static SafetyHookMid GameplayCameraFOVInstruction4Hook{};

enum destInstruction
{
	FADD,
	EAX,
	EDX,
	ECX
};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float fovFactor, destInstruction DestInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fovFactor;

	switch (DestInstruction)
	{
		case FADD:
		{
			FPU::FADD(fNewCameraFOV);
			break;
		}

		case EAX:
		{
			ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}

		case EDX:
		{
			ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}

		case ECX:
		{
			ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}
	}
}

void FOVFix()
{
	if (eGameType == Game::PD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "8B 54 24 ?? D8 07", "8B 44 24 ?? D8 44 24 ?? 50", "8B 4C 24 ?? D8 44 24 ?? 51", "8B 4D ?? 51 8B 4C 24", "D8 05 ?? ?? ?? ?? D9 1C ?? 51 8B 8C 24", 
		"8B 44 24 ?? 50 8D 4C 24 ?? 51 8B 8C 24 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 FF 15 ?? ?? ?? ?? 81 C4 ?? ?? ?? ?? C2 ?? ?? D9 44 24 ?? D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? DD D8 C7 44 24 ?? ?? ?? ?? ?? EB ?? 90 90 90 90 90 90 90 90 90 90 83 EC",
		"8B 44 24 ?? 50 8D 4C 24 ?? 51 8B 8C 24 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 FF 15 ?? ?? ?? ?? 81 C4 ?? ?? ?? ?? C2 ?? ?? D9 44 24 ?? D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? DD D8 C7 44 24 ?? ?? ?? ?? ?? EB ?? 90 90 90 90 90 90 90 90 90 90 90",
		"8B 44 24 ?? 50 8D 4C 24 ?? 51 8B 8C 24 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 FF 15 ?? ?? ?? ?? 81 C4 ?? ?? ?? ?? C2 ?? ?? D9 44 24 ?? D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? DD D8 C7 44 24 ?? ?? ?? ?? ?? EB ?? 90 90 90 90 90 90 90 90 90 90 A1");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Main Menu Camera FOV Instruction: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[MainMenuFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[CutscenesFOV1] - (std::uint8_t*)dllModule2);

			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[CutscenesFOV2] - (std::uint8_t*)dllModule2);

			spdlog::info("Cutscenes Camera FOV Instruction 3: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[CutscenesFOV3] - (std::uint8_t*)dllModule2);

			spdlog::info("Gameplay Camera FOV Instruction 1: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV1] - (std::uint8_t*)dllModule2);

			spdlog::info("Gameplay Camera FOV Instruction 2: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV2] - (std::uint8_t*)dllModule2);

			spdlog::info("Gameplay Camera FOV Instruction 3: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV3] - (std::uint8_t*)dllModule2);

			spdlog::info("Gameplay Camera FOV Instruction 4: Address is powerdrome.dll+{:x}", CameraFOVInstructionsScansResult[GameplayFOV4] - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MainMenuFOV], 4);

			MainMenuCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MainMenuFOV], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x54, 1.0f, EDX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV1], 4);

			CutscenesCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x2C, 1.0f, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV2], 4);

			CutscenesCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x4, 1.0f, ECX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOV3], 3);

			CutscenesCameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV3], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebp + 0x34, 1.0f, ECX, ctx);
			});

			GameplayFOVAddress1 = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[GameplayFOV1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV1], 6);

			GameplayCameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook((uintptr_t)GameplayFOVAddress1, fFOVFactor, FADD, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV2], 4);

			GameplayCameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x14, fFOVFactor, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV3], 4);

			GameplayCameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV3], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x4, fFOVFactor, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV4], 4);

			GameplayCameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV4], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x10, fFOVFactor, EAX, ctx);
			});			
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