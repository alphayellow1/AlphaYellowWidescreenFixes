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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "WorldWarZeroIronStormWidescreenFix";
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

// Ini variables
bool bFixActive;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fAspectRatioScale;
float fFOVFactor;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
static uint8_t* ResolutionWidth1Address;
static uint8_t* ResolutionHeight1Address;
static uint8_t* ResolutionWidth2Address;
static uint8_t* ResolutionHeight2Address;
static uint8_t* ResolutionWidth3Address;
static uint8_t* ResolutionHeight3Address;
static uint8_t* ResolutionWidth4Address;
static uint8_t* ResolutionHeight4Address;

// Game detection
enum class Game
{
	WWZIS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WWZIS, {"World War Zero: Iron Storm", "WorldWarZero.exe"}},
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
	return atanf(tanf(fCurrentFOV) * fAspectRatioScale);
}

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid CameraFOVInstruction1Hook{};

void CameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV1 = *reinterpret_cast<float*>(ctx.eax + 0x4C);

	if (fCurrentCameraHFOV1 == 0.7853981853f)
	{
		fNewCameraFOV1 = CalculateNewFOV(fCurrentCameraHFOV1) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV1 = CalculateNewFOV(fCurrentCameraHFOV1);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV1]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x4C);

	if (fCurrentCameraFOV2 == 0.7853981853f)
	{
		fNewCameraFOV2 = CalculateNewFOV(fCurrentCameraFOV2) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV2 = CalculateNewFOV(fCurrentCameraFOV2);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(ctx.eax + 0x4C);
	
	if (fCurrentCameraFOV3 == 0.7853981853f)
	{
		fNewCameraFOV3 = CalculateNewFOV(fCurrentCameraFOV3) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV3 = CalculateNewFOV(fCurrentCameraFOV3);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV3]
	}
}

static SafetyHookMid CameraFOVInstruction4Hook{};

void CameraFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV4 = *reinterpret_cast<float*>(ctx.eax + 0x4C);

	if (fCurrentCameraFOV4 == 0.7853981853f)
	{
		fNewCameraFOV4 = CalculateNewFOV(fCurrentCameraFOV4) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV4 = CalculateNewFOV(fCurrentCameraFOV4);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV4]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::WWZIS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "89 35 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ??");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth1Address = Memory::GetAddress32(ResolutionInstructions1ScanResult + 8);

			ResolutionHeight1Address = Memory::GetAddress32(ResolutionInstructions1ScanResult + 2);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions1MidHook{};

			ResolutionInstructions1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 33 C0 89 15 ?? ?? ?? ??");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth2Address = Memory::GetAddress32(ResolutionInstructions2ScanResult + 1);

			ResolutionHeight2Address = Memory::GetAddress32(ResolutionInstructions2ScanResult + 9);

			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth2Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 7, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 7, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight2Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 8B 3A 52");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);
			
			ResolutionWidth3Address = Memory::GetAddress32(ResolutionInstructions3ScanResult + 1);
			
			ResolutionHeight3Address = Memory::GetAddress32(ResolutionInstructions3ScanResult + 7);
			
			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid ResolutionInstructions3MidHook{};

			ResolutionInstructions3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth3Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight3Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "89 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 08 84 C0");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);
			
			ResolutionWidth4Address = Memory::GetAddress32(ResolutionInstructions4ScanResult + 2);
			
			ResolutionHeight4Address = Memory::GetAddress32(ResolutionInstructions4ScanResult + 7);
			
			Memory::PatchBytes(ResolutionInstructions4ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);
			
			static SafetyHookMid ResolutionInstructions4MidHook{};
			
			ResolutionInstructions4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth4Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight4Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions 4 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "89 1E 89 6E 04 C6 46 46 01 C6 46 45 00 C6 46 44 00");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionInstructions5ScanResult, "\x90\x90\x90\x90\x90", 5);
			
			static SafetyHookMid ResolutionInstructions5MidHook{};
			
			ResolutionInstructions5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0x4) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "89 4E 08 89 56 0C C6 46 45 01 C6 46 44 00 5F 5E");
		if (ResolutionInstructions6ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionInstructions6ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid ResolutionInstructions6MidHook{};
			
			ResolutionInstructions6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x8) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResY;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions 6 scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 74 06 D9 C9 D9 E0 D9 C9");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 40 4C D9 F2 DD D8 D9 58 68 A1 ?? ?? ?? ??");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90", 3);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, CameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 40 4C D9 FF D8 3D ?? ?? ?? ?? D9 58 64 D9 05 ?? ?? ?? ??");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90", 3);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D9 40 4C 8B 08 D9 F2 8B 50 04 89 4C 24 14 8B 48 08 89 54 24 18 8B 50 14 89 4C 24 1C 8B 48 20 89 54 24 20");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction3ScanResult, "\x90\x90\x90", 3);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult, CameraFOVInstruction3MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction4ScanResult = Memory::PatternScan(exeModule, "D9 40 4C 89 4C 24 28 D9 F2 8B 48 04 89 4C 24 1C 89 54 24 20 DD D8 D9 40 50 D9 F2");
		if (CameraFOVInstruction4ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction4ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction4ScanResult, "\x90\x90\x90", 3);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstruction4ScanResult, CameraFOVInstruction4MidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction 4 memory address.");
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