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
std::string sFixName = "WorldChampionshipSnooker2004WidescreenFix";
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
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fCurrentCameraFOV;
float fNewCameraFOV;
uint8_t* ResolutionWidth1Address;
uint8_t* ResolutionHeight1Address;
uint8_t* ResolutionWidth2Address;
uint8_t* ResolutionHeight2Address;
uint8_t* ResolutionWidth3Address;
uint8_t* ResolutionHeight3Address;
uint8_t* ResolutionWidth4Address;
uint8_t* ResolutionHeight4Address;
uint8_t* ResolutionWidth5Address;
uint8_t* ResolutionHeight5Address;
uint8_t* ResolutionWidth6Address;
uint8_t* ResolutionHeight6Address;
uint8_t* ResolutionWidth7Address;
uint8_t* ResolutionHeight7Address;
uint8_t* ResolutionWidth8Address;
uint8_t* ResolutionHeight8Address;
uint8_t* ResolutionWidth9Address;
uint8_t* ResolutionHeight9Address;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	WCS2004,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WCS2004, {"World Championship Snooker 2004", "wcs2004.exe"}},
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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul dword ptr ds:[fNewAspectRatio]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::WCS2004 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 8B 44 24 28 A3 ?? ?? ?? ?? 8B 44 24 1C 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth1Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight1Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 20, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions1ScanResult + 18, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult + 18, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "8B 54 24 18 A3 ?? ?? ?? ?? 8B 44 24 1C 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 15, Memory::PointerMode::Absolute);

			ResolutionHeight3Address = Memory::GetPointer<uint32_t>(ResolutionInstructions2ScanResult + 5, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 13, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 13, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth3Address) = iCurrentResX;				
			});

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 4, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 4, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight3Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 8B 44 24 30 5E 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 12, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction3MidHook{};

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth4Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions3ScanResult + 10, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction3MidHook{};

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult + 10, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight4Address) = iCurrentResY;
			});			
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "A3 18 20 6A 00 33 C0 5E C2 10 00 3D ED 03 00 00 0F 85 16 01 00 00 A1 1C 20 6A 00 83 F0 01 5F A3 1C 20 6A 00 33 C0 5E C2 10 00 A1 20 20 6A 00 83 F0 01 5F A3 20 20 6A 00 33 C0 5E C2 10 00 2D EF 03 00 00 83 F8 35 0F 87 E0 00 00 00 0F B6 90 D4 34 40 00 FF 24 95 C0 34 40 00 C1 E9 10 49 0F 85 C8 00 00 00 8B 74 24 0C 8B 3D E0 52 64 00 6A 00 6A 00 68 47 01 00 00 68 24 04 00 00 56 FF D7 6A 00 50 68 50 01 00 00 68 24 04 00 00 56 FF D7 6A 00 6A 00 68 03 05 00 00 56 A3 00 20 6A 00 FF 15 DC 52 64 00 5F 33 C0 5E C2 10 00 C1 E9 10 49 75 7B 8B 74 24 0C 8B 3D E0 52 64 00 6A 00 6A 00 68 47 01 00 00 68 04 04 00 00 56 FF D7 6A 00 50 68 50 01 00 00 68 04 04 00 00 56 FF D7 8B 0D 00 20 6A 00 68 24 20 6A 00 68 0C 20 6A 00 68 08 20 6A 00 68 04 20 6A 00 8B D0 E8 BC 3B 00 00 5F 33 C0 5E C2 10 00 A1 14 20 6A 00 83 F0 01 5F A3 14 20 6A 00");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth5Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight5Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 270, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions4ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction4MidHook{};

			ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth5Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 269, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction4MidHook{};

			ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 269, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight5Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 4 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 ED A3 ?? ?? ?? ??");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions5ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions5ScanResult + 19, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions5ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction5MidHook{};

			ResolutionWidthInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth7Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions5ScanResult + 18, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstructionMidHook{};

			ResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult + 18, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight7Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? B8 01 00 00 00 C3 CC CC CC CC CC CC CC CC CC CC CC CC CC CC 89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ??");
		if (ResolutionInstructions6ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 8, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions6ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions6MidHook{};

			ResolutionInstructions6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth8Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight8Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 6 scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 89 F8 08 6B 00 8B B1 E8 08 6B 00 8B B9 EC 08 6B 00 8B 99 F0 08 6B 00");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 44 24 04 A3 ?? ?? ?? ?? B8 01 00 00 00 C2 04 00 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 8B 44 24 04");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVAddress = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 5, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult + 4, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 4, [](SafetyHookContext& ctx)
			{
				fCurrentCameraFOV = std::bit_cast<float>(ctx.eax);

				fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

				*reinterpret_cast<float*>(CameraFOVAddress) = fNewCameraFOV;
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
