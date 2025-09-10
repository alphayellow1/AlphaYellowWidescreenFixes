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
std::string sFixName = "ArthurAndTheInvisiblesWidescreenFix";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewAspectRatio2;

// Game detection
enum class Game
{
	AATI,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::AATI, {"Arthur and the Invisibles", "GameModule.elb"}},
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

static SafetyHookMid ResolutionWidthInstruction5Hook{};

void ResolutionWidthInstruction5MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fild dword ptr ds:[iCurrentResX]
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x14);

	fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

static SafetyHookMid HUDAspectRatioInstruction1Hook{};

void HUDAspectRatioInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewAspectRatio2]
	}
}

static SafetyHookMid HUDAspectRatioInstruction2Hook{};

void HUDAspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio2]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::AATI && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "8B 0A 89 0D E4 57 74 00 8B 6A 04 89 2D E8 57 74 00");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90", 2);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructions1ScanResult + 8, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult + 8, [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "8B 08 89 0D 40 59 74 00 8B 50 04 89 15 44 59 74 00");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90", 2);
			
			static SafetyHookMid ResolutionWidthInstruction2MidHook{};
			
			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});
			
			Memory::PatchBytes(ResolutionInstructions2ScanResult + 8, "\x90\x90\x90", 3);
			
			static SafetyHookMid ResolutionHeightInstruction2MidHook{};
			
			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "8B 44 24 20 8B 4C 24 24 A3 E4 57 74 00 89 0D E8 57 74 00 8B 94 24 80 00 00 00");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			static SafetyHookMid ResolutionInstructions3MidHook{};

			ResolutionInstructions3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "8B 44 24 20 8B 4C 24 24 A3 E4 57 74 00 89 0D E8 57 74 00 8B 54 24 68");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions4ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			static SafetyHookMid ResolutionInstructions4MidHook{};

			ResolutionInstructions4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 4 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "DB 41 0C 8B 51 10 89 54 24 14");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions5ScanResult + 3, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction5MidHook{};

			ResolutionHeightInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult + 3, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructions5ScanResult, "\x90\x90\x90", 3);

			ResolutionWidthInstruction5Hook = safetyhook::create_mid(ResolutionInstructions5ScanResult, ResolutionWidthInstruction5MidHook);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "74 17 8B 46 0C 8B 0D 40 59 74 00 3B C1 74 0A 0F AF C1 33 D2 F7 F7 89 46 0C 8B 3D E8 57 74 00 85 FF 74 17 8B 46 10 8B 0D 44 59 74 00 3B C1 74 0A 0F AF C1 33 D2 F7 F7 89 46 10");
		if (ResolutionInstructions6ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 1, "\x14", 1);

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 14, "\x07", 1);

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 34, "\x14", 1);

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 47, "\x07", 1);

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 22, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionWidthInstruction6MidHook{};

			ResolutionWidthInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult + 22, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;				
			});

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 55, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction6MidHook{};

			ResolutionHeightInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult + 55, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 6 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions7ScanResult = Memory::PatternScan(exeModule, "89 56 0C 89 46 10 8B 0D CC D6 73 00");
		if (ResolutionInstructions7ScanResult)
		{
			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions7ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions7ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionInstructions7MidHook{};

			ResolutionInstructions7MidHook = safetyhook::create_mid(ResolutionInstructions7ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 7 scan memory address.");
			return;
		}

		fNewAspectRatio2 = 0.75f / fAspectRatioScale;

		std::uint8_t* CameraAspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "8B 15 1C 1C 6D 00 89 51 38 EB 3C 8B 41 04");
		if (CameraAspectRatioInstructionScanResult)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraAspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(CameraAspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraAspectRatioInstructionMidHook{};

			CameraAspectRatioInstructionMidHook = safetyhook::create_mid(CameraAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(fNewAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* HUDAspectRatioInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 05 88 3D 6D 00 8B 48 38 8D 44 24 2C 68 D0 1A 70 00 50 8B 51 18");
		if (HUDAspectRatioInstruction1ScanResult)
		{
			spdlog::info("HUD Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstruction1ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(HUDAspectRatioInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			HUDAspectRatioInstruction1Hook = safetyhook::create_mid(HUDAspectRatioInstruction1ScanResult, HUDAspectRatioInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate HUD aspect ratio instruction 1 memory address.");
			return;
		}

		std::uint8_t* HUDAspectRatioInstruction2ScanResult = Memory::PatternScan(exeModule, "D8 0D 88 3D 6D 00 33 C0 66 8B 86 94 08 00 00 3B C1 D9 5C 24 1C");
		if (HUDAspectRatioInstruction2ScanResult)
		{
			spdlog::info("HUD Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), HUDAspectRatioInstruction2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(HUDAspectRatioInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			HUDAspectRatioInstruction2Hook = safetyhook::create_mid(HUDAspectRatioInstruction2ScanResult, HUDAspectRatioInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate HUD aspect ratio instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 14 D8 0D 28 1C 6D 00 8B 41 18 D8 0D 10 13 6D 00 D9 F2");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90", 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
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
