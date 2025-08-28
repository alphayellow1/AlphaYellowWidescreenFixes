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
std::string sFixName = "UrbanFreestyleSoccerWidescreenFix";
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

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraFOV;
float fAspectRatioScale;
int iNewCameraFOV1;
int iNewCameraFOV2;

// Game detection
enum class Game
{
	UFS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::UFS, {"Urban Freestyle Soccer", "PlayUFS.exe"}},
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
	int& iCurrentCameraFOV1 = *reinterpret_cast<int*>(ctx.edi + 0xC0);

	iNewCameraFOV1 = (int)(iCurrentCameraFOV1 * fFOVFactor);

	_asm
	{
		fild dword ptr ds:[iNewCameraFOV1]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	int& iCurrentCameraFOV2 = *reinterpret_cast<int*>(ctx.esi + 0xC0);

	iNewCameraFOV2 = (int)(iCurrentCameraFOV2 * fFOVFactor);

	_asm
	{
		fild dword ptr ds:[iNewCameraFOV2]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::UFS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "89 4E 78 8B 57 04 89 56 7C");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Renderer Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90\x90", 3);

			static SafetyHookMid RendererResolutionWidthInstructionMidHook{};

			RendererResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x78) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions1ScanResult + 6, "\x90\x90\x90", 3);

			static SafetyHookMid RendererResolutionHeightInstructionMidHook{};

			RendererResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x7C) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "89 8E A8 00 00 00 8A 46 30 84 C0 89 54 24 0C 8D 46 40 75 03 8D 46 78 8B 08 8B 50 08 89 4C 24 04 8B 48 04 8B 40 0C 89 44 24 10 89 8E AC 00 00 00");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid RendererResolution2WidthInstructionMidHook{};
			
			RendererResolution2WidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xA8) = iCurrentResX;
			});
			
			Memory::PatchBytes(ResolutionInstructions2ScanResult + 42, "\x90\x90\x90\x90\x90\x90", 6);
			
			static SafetyHookMid RendererResolution2HeightInstructionMidHook{};
			
			RendererResolution2HeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 42, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xAC) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction1ScanResult = Memory::PatternScan(exeModule, "C7 86 C4 00 00 00 39 8E E3 3F EB 0A C7 86 C4 00 00 00 AB AA AA 3F");
		if (AspectRatioInstruction1ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction1ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(AspectRatioInstruction1ScanResult + 6, fNewAspectRatio);

			Memory::Write(AspectRatioInstruction1ScanResult + 18, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(exeModule, "C7 05 F4 73 6C 00 39 8E E3 3F EB 0A C7 05 F4 73 6C 00 AB AA AA 3F");
		if (AspectRatioInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstruction2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(AspectRatioInstruction2ScanResult + 6, fNewAspectRatio);
			
			Memory::Write(AspectRatioInstruction2ScanResult + 18, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "DB 87 C0 00 00 00 D8 0D ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 87 CC 00 00 00");
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

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "DB 86 C0 00 00 00 D8 0D ?? ?? ?? ?? DC 0D ?? ?? ?? ?? D9 F2");
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
