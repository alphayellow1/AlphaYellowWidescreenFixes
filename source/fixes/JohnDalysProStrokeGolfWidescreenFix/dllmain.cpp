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
std::string sFixName = "JohnDalysProStrokeGolfWidescreenFix";
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
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
uint8_t* ResolutionWidth1Address;
uint8_t* ResolutionHeight1Address;
uint8_t* ResolutionWidth2Address;
uint8_t* ResolutionHeight2Address;
uint8_t* ResolutionWidth3Address;
uint8_t* ResolutionHeight3Address;
uint8_t* ResolutionWidth4Address;
uint8_t* ResolutionHeight4Address;

// Game detection
enum class Game
{
	JDPSG,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::JDPSG, {"John Daly's ProStroke Golf", "John Daly's ProStroke Golf.exe"}},
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

static SafetyHookMid CameraFOVInstruction1Hook{};

void CameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.edi + 0x114);

	fNewCameraFOV1 = fCurrentCameraFOV1 * fFOVFactor;

	_asm
	{
		fmul dword ptr ds:[fNewCameraFOV1]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x114);

	fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::JDPSG && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		/*
		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "A3 3C 2E A0 00 E8 71 D6 17 00 83 3D 78 2E A0 00 00 A3 40 2E A0 00");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth1Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions1ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight1Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions1ScanResult + 18, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions1ScanResult + 17, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult + 17, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}
		*/
		
		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "89 4E 78 8B 57 04 89 56 7C 8B 47 0C 89 86 8C 00 00 00");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90", 3);
			
			static SafetyHookMid ResolutionWidthInstruction2MidHook{};
			
			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x78) = iCurrentResX;
			});
			
			Memory::PatchBytes(ResolutionInstructions2ScanResult + 6, "\x90\x90\x90", 3);
			
			static SafetyHookMid ResolutionHeightInstruction2MidHook{};
			
			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x7C) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "89 8E B0 00 00 00 80 7E 30 00 89 44 24 14 89 54 24 10 8D 46 40 75 03 8D 46 78 8B 08 8B 50 08 89 4C 24 08 8B 48 04 8B 40 0C 89 8E B4 00 00 00");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction3MidHook{};

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xB0) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions3ScanResult + 41, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction3MidHook{};

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult + 41, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xB4) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "A3 A0 D4 A2 00 DB 86 B4 00 00 00 8B 8E B4 00 00 00 85 C9 7D 06 D8 05 50 88 84 00 E8 8D 1F 18 00 A3 A4 D4 A2 00");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth2Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions4ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight2Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions4ScanResult + 33, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions4ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction4MidHook{};

			ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth2Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 32, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction4MidHook{};

			ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 32, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight2Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 4 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "89 AE F4 00 00 00 88 9E A6 00 00 00 88 9E A7 00 00 00 88 96 A8 00 00 00 88 9E AB 00 00 00 88 9E AC 00 00 00 88 8E AD 00 00 00 88 9E AE 00 00 00 88 9E AF 00 00 00 89 9E B4 00 00 00 89 86 EC 00 00 00 89 BE F0 00 00 00");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions5ScanResult + 66, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction5MidHook{};

			ResolutionWidthInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult + 66, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xF0) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions5ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction5MidHook{};

			ResolutionHeightInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xF4) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}
		
		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D8 8F 14 01 00 00 D9 5C 24 0C D9 87 20 01 00 00");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, CameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 86 14 01 00 00 DC 0D ?? ?? ?? ?? D9 5C 24 18 D9 44 24 18");
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
