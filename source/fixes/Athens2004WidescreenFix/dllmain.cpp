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
std::string sFixName = "Athens2004WidescreenFix";
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
float fNewAspectRatio;
float fAspectRatioScale;
float fFOVFactor;
float fCurrentCameraFOV;
float fNewCameraFOV;
static uint8_t* MainMenuResolutionWidthAddress;
static uint8_t* MainMenuResolutionHeightAddress;
static uint8_t* MainMenuBitDepthAddress;
static uint8_t* GameplayResolutionWidthAddress;
static uint8_t* GameplayResolutionHeightAddress;
static uint8_t* GameplayBitDepthAddress;

// Game detection
enum class Game
{
	ATHENS2004,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::ATHENS2004, {"Athens 2004", "Athens.exe"}},
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

void WidescreenFix()
{
	if (eGameType == Game::ATHENS2004 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* MainMenuResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "66 A1 ?? ?? ?? ?? 66 8B 0D ?? ?? ?? ?? 66 8B 15 ?? ?? ?? ??");
		if (MainMenuResolutionInstructionsScanResult)
		{
			spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), MainMenuResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			MainMenuResolutionWidthAddress = Memory::GetPointer<uint32_t>(MainMenuResolutionInstructionsScanResult + 2, Memory::PointerMode::Absolute);

			MainMenuResolutionHeightAddress = Memory::GetPointer<uint32_t>(MainMenuResolutionInstructionsScanResult + 9, Memory::PointerMode::Absolute);

			MainMenuBitDepthAddress = Memory::GetPointer<uint32_t>(MainMenuResolutionInstructionsScanResult + 16, Memory::PointerMode::Absolute);

			static SafetyHookMid MainMenuResolutionWidthInstructionMidHook{};

			MainMenuResolutionWidthInstructionMidHook = safetyhook::create_mid(MainMenuResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentMainMenuWidth = *reinterpret_cast<int*>(MainMenuResolutionWidthAddress);

				iCurrentMainMenuWidth = iCurrentResX;
			});

			static SafetyHookMid MainMenuResolutionHeightInstructionMidHook{};

			MainMenuResolutionHeightInstructionMidHook = safetyhook::create_mid(MainMenuResolutionInstructionsScanResult + 6, [](SafetyHookContext& ctx)
			{
				int& iCurrentMainMenuHeight = *reinterpret_cast<int*>(MainMenuResolutionHeightAddress);

				iCurrentMainMenuHeight = iCurrentResY;
			});

			static SafetyHookMid MainMenuBitDepthInstructionMidHook{};

			MainMenuBitDepthInstructionMidHook = safetyhook::create_mid(MainMenuResolutionInstructionsScanResult + 13, [](SafetyHookContext& ctx)
			{
				int& iCurrentMainMenuBitDepth = *reinterpret_cast<int*>(MainMenuBitDepthAddress);

				iCurrentMainMenuBitDepth = 32;
			});
		}
		else
		{
			spdlog::error("Failed to locate main menu resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* GameplayResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "66 8B 0D ?? ?? ?? ?? 66 A1 ?? ?? ?? ?? 66 8B 15 ?? ?? ?? ??");
		if (GameplayResolutionInstructionsScanResult)
		{
			spdlog::info("Gameplay Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), GameplayResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			GameplayResolutionWidthAddress = Memory::GetPointer<uint32_t>(GameplayResolutionInstructionsScanResult + 9, Memory::PointerMode::Absolute);

			GameplayResolutionHeightAddress = Memory::GetPointer<uint32_t>(GameplayResolutionInstructionsScanResult + 3, Memory::PointerMode::Absolute);

			GameplayBitDepthAddress = Memory::GetPointer<uint32_t>(GameplayResolutionInstructionsScanResult + 16, Memory::PointerMode::Absolute);
			
			static SafetyHookMid GameplayResolutionWidthInstructionMidHook{};

			GameplayResolutionWidthInstructionMidHook = safetyhook::create_mid(GameplayResolutionInstructionsScanResult + 7, [](SafetyHookContext& ctx)
			{
				int& iCurrentGameplayWidth = *reinterpret_cast<int*>(GameplayResolutionWidthAddress);

				iCurrentGameplayWidth = iCurrentResX;
			});

			static SafetyHookMid GameplayResolutionHeightInstructionMidHook{};

			GameplayResolutionHeightInstructionMidHook = safetyhook::create_mid(GameplayResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentGameplayHeight = *reinterpret_cast<int*>(GameplayResolutionHeightAddress);

				iCurrentGameplayHeight = iCurrentResY;
			});
			
			static SafetyHookMid GameplayBitDepthInstructionMidHook{};

			GameplayBitDepthInstructionMidHook = safetyhook::create_mid(GameplayResolutionInstructionsScanResult + 13, [](SafetyHookContext& ctx)
			{
				int& iCurrentGameplayBitDepth = *reinterpret_cast<int*>(GameplayBitDepthAddress);

				iCurrentGameplayBitDepth = 32;
			});
		}
		else
		{
			spdlog::error("Failed to locate gameplay resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 47 18 E8 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D9 C1 DA E9 59 59 DF E0");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV = std::bit_cast<float>(ctx.eax);

				if (fCurrentCameraFOV == 0.1003965363f || fCurrentCameraFOV == 0.2991908491f || fCurrentCameraFOV == 0.37890625f || fCurrentCameraFOV == 0.3966405988f || fCurrentCameraFOV == 0.7853981853f)
				{
					fNewCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraFOV, fAspectRatioScale);
				}
				else
				{
					fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;
				}

				*reinterpret_cast<float*>(ctx.edi + 0x18) = fNewCameraFOV;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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