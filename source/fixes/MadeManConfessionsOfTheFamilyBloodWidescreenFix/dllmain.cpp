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
std::string sFixName = "MadeManConfessionsOfTheFamilyBloodWidescreenFix";
std::string sFixVersion = "1.2";
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
float fNewCameraFOV;
float fFOVFactor;
float fNewAspectRatio;
float fAspectRatioScale;
static float fCurrentCameraFOV;

// Game detection
enum class Game
{
	MM,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MM, {"Made Man: Confessions of the Family Blood", "IMM.exe"}},
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

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	if (fCurrentCameraFOV == 1.1344640254974365f)
	{
		fNewCameraFOV = (1.0f / fAspectRatioScale) * (1.0f / fFOVFactor);
	}
	else
	{
		fNewCameraFOV = (1.0f / fAspectRatioScale);
	}

	_asm
	{
		fdivr dword ptr ds:[fNewCameraFOV]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	if (fCurrentCameraFOV == 1.1344640254974365f)
	{
		fNewCameraFOV = (1.0f / fAspectRatioScale) * (1.0f / fFOVFactor);
	}
	else
	{
		fNewCameraFOV = (1.0f / fAspectRatioScale);
	}

	_asm
	{
		fdivr dword ptr ds:[fNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::MM && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstruction1ScanResult = Memory::PatternScan(exeModule, "8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 81 EC 00 02 00 00");
		if (ResolutionInstruction1ScanResult)
		{
			spdlog::info("Resolution Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstruction1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstruction1ScanResult + 6, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstruction1ScanResult + 6, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instruction 1 memory address.");
			return;
		}

		std::uint8_t* ResolutionInstruction2ScanResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 50 51 52 8D 44 24 14");
		if (ResolutionInstruction2ScanResult)
		{
			spdlog::info("Resolution Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstruction2ScanResult + 6, "\x90\x90\x90\x90\x90\x90", 6);			

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstruction2ScanResult + 6, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instruction 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioScanResult = Memory::PatternScan(exeModule, "3B CD CC 4C BD 00 00 00 00 00 00 E0 3F AB AA AA 3F 00 00 80 BE CD CC CC BC 9A");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioScanResult - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioScanResult + 13, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio memory address.");
			return;
		}

		

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 43 48 8B 44 24 20 DC 0D ?? ?? ?? ?? 2B C6 85 C0 D9 F2");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);
			
			static SafetyHookMid CameraFOVInstruction1MidHook{};
			
			CameraFOVInstruction1MidHook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, [](SafetyHookContext& ctx)
			{
				fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ebx + 0x48);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D8 3D ?? ?? ?? ?? D9 54 24 34 D8 4C 24 10 D9 5C 24 10 7E 45 0F 28 D0 F3 0F 5E 54 24 34");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D8 3D ?? ?? ?? ?? D9 54 24 34 D8 4C 24 10 D9 5C 24 10 7E 43 0F 28 D0 F3 0F 5E 54 24 34 0F 28 E2 F3 0F 5E E5");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult, CameraFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 3 memory address.");
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