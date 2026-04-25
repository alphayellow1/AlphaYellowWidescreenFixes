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

// Fix details
std::string sFixName = "KISSPsychoCircusTheNightmareChildFOVFix";
std::string sFixVersion = "1.8";
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
uintptr_t CameraHFOVAddress;
uintptr_t CameraVFOVAddress;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fNewInventoryHFOV;
float fNewInventoryVFOV;

// Game detection
enum class Game
{
	KPCTNC,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	GameplayHFOV,
	GameplayVFOV,
	InventoryFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::KPCTNC, {"KISS Psycho Circus: The Nightmare Child", "client.exe"}},
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

	dllModule2 = Memory::GetHandle("cshell.dll");

	return true;
}

static SafetyHookMid GameplayHFOVInstructionHook{};
static SafetyHookMid GameplayVFOVInstructionHook{};
static SafetyHookMid InventoryFOVInstructionsHook{};

void FOVFix()
{
	if (eGameType == Game::KPCTNC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? D9 1C ?? 50", "D9 05 ?? ?? ?? ?? 8B 06", "8B 4C 24 ?? D8 4C 24 ?? D8 F1");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Camera HFOV Instruction: Address is cshell.dll+{:x}", CameraFOVInstructionsScansResult[GameplayHFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Gameplay Camera VFOV Instruction: Address is cshell.dll+{:x}", CameraFOVInstructionsScansResult[GameplayVFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Inventory Selection FOV Instructions Scan: Address is cshell.dll+{:x}", CameraFOVInstructionsScansResult[InventoryFOV] - (std::uint8_t*)dllModule2);

			CameraHFOVAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[GameplayHFOV] + 2, Memory::PointerMode::Absolute);

			CameraVFOVAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[GameplayVFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayHFOV], 6);

			GameplayHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayHFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = Memory::ReadMem(CameraHFOVAddress);

				fNewCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, fAspectRatioScale, fFOVFactor);

				FPU::FLD(fNewCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayVFOV], 6);

			GameplayVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayVFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = Memory::ReadMem(CameraVFOVAddress);

				fNewCameraVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentCameraVFOV * fAspectRatioScale, fFOVFactor);

				FPU::FLD(fNewCameraVFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[InventoryFOV], 8);

			InventoryFOVInstructionsHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[InventoryFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentInventoryFOV = Memory::ReadMem(ctx.esp + 0x24);

				fNewInventoryHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentInventoryFOV, fAspectRatioScale);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewInventoryHFOV);

				fNewInventoryVFOV = fCurrentInventoryFOV * fAspectRatioScale;

				FPU::FMUL(fNewInventoryVFOV);
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