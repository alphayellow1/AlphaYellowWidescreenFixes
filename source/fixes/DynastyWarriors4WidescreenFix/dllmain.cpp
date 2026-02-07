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
std::string sFixName = "DynastyWarriors4WidescreenFix";
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
float fNewCameraFOV;

// Game detection
enum class Game
{
	DW4_EN,
	DW4_JPN,
	DW4_KOREAN,
	DW4_CHN,
	Unknown
};

enum ResolutionInstructionsIndex
{
	GameplayFOVScan,
	MenuFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DW4_EN, {"Dynasty Warriors 4 (English)", "Dynasty Warriors 4 Hyper.exe"}},
	{Game::DW4_JPN, {"Dynasty Warriors 4 (Japanese)", "Shin Sangokumusou 3.exe"}},
	{Game::DW4_KOREAN, {"Dynasty Warriors 4 (Korean)", "Samgukmussang 3 Hyper.exe"}},
	{Game::DW4_CHN, {"Dynasty Warriors 4 (Chinese)", "Zhen SanGuoWuShuang 3 Hyper.exe"}},
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

static SafetyHookMid CharactersNameAspectRatioInstructionHook{};
static SafetyHookMid GameplayCameraFOVInstructionHook{};
static SafetyHookMid MenuCameraFOVInstructionHook{};

void CameraFOVInstructionsMidHook(float& SourceAddress, uintptr_t DestAddress, float fovFactor = 1.0f)
{
	float& fCurrentCameraFOV = SourceAddress;

	fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fovFactor;

	*reinterpret_cast<float*>(DestAddress) = fNewCameraFOV;
}

void WidescreenFix()
{
	if ((eGameType == Game::DW4_EN || eGameType == Game::DW4_JPN || eGameType == Game::DW4_KOREAN || eGameType == Game::DW4_CHN) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListUnlockScanResult = Memory::PatternScan(exeModule, "0F 82 ?? ?? ?? ?? 8B 53");
		if (ResolutionListUnlockScanResult)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionListUnlockScanResult, "\xEB\x2F"); // jump unconditionally rather than if it's below 640 to always allow custom resolutions
		}
		else
		{
			spdlog::error("Failed to locate resolution list unlock scan memory address.");
			return;
		}

		std::uint8_t* CharactersNameAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 44 24 ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB ?? 83 FF ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB ?? 83 FF ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB ?? 83 FF ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB");
		if (CharactersNameAspectRatioInstructionScanResult)
		{
			spdlog::info("Characters Name Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), CharactersNameAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(CharactersNameAspectRatioInstructionScanResult, 8); // NOP out the original instruction
			
			CharactersNameAspectRatioInstructionHook = safetyhook::create_mid(CharactersNameAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = fNewAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate character name aspect ratio instruction memory address.");
			return;
		}
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "F3 0F 11 85 ?? ?? ?? ?? 8B 17 89 10 8B 4F ??", "F3 0F 11 9E ?? ?? ?? ?? F3 0F 10 1D ?? ?? ?? ?? F3 0F 11 9E ?? ?? ?? ?? F3 0F 10 1D ?? ?? ?? ?? F3 0F 11 9E ?? ?? ?? ?? F3 0F 10 1D ?? ?? ?? ?? F3 0F 11 9E ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Menu Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MenuFOVScan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOVScan], 8);
			
			GameplayCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], ctx.ebp + 0x98, fFOVFactor);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MenuFOVScan], 8);

			MenuCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MenuFOVScan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.xmm3.f32[0], ctx.esi + 0x98);
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
