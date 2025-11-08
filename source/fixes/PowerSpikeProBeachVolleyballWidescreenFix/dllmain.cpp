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
std::string sFixName = "PowerSpikeProBeachVolleyballWidescreenFix";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
double dNewAspectRatio2;
uint8_t* CameraFOVOffset;

// Game detection
enum class Game
{
	PSPBV,
	Unknown
};

enum ResolutionInstructionsIndex
{
	RendererResolutionScan,
	ViewportResolutionScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PSPBV, {"Power Spike: Pro Beach Volleyball", "PowerSpike.exe"}},
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

static SafetyHookMid RendererResolutionWidthInstructionMidHook{};
static SafetyHookMid RendererResolutionHeightInstructionMidHook{};
static SafetyHookMid ViewportResolutionWidthInstructionMidHook{};
static SafetyHookMid ViewportResolutionHeightInstructionMidHook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fmul qword ptr ds:[dNewAspectRatio2]
	}
}

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.edx * 0x4 + CameraFOVOffset);

	fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::PSPBV && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "89 4A 0C 8B 85 58 FF FF FF 8B 8D 58 FF FF FF 8B 50 20 2B 51 18 8B 85 58 FF FF FF 89 50 10", "8B 54 11 0C 89 15 6C 65 6A 00 8B 45 10 6B C0 7C 8B 0D 48 40 4A 00 8B 91 BC 04 00 00 8B 44 02 08");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolutionScan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructionsScansResult[RendererResolutionScan], "\x90\x90\x90", 3);

			RendererResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edx + 0xC) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[RendererResolutionScan] + 27, "\x90\x90\x90", 3);

			RendererResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan] + 27, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax + 0x10) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutionScan], "\x90\x90\x90\x90", 4);

			ViewportResolutionWidthInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutionScan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolutionScan] + 28, "\x90\x90\x90\x90", 4);

			ViewportResolutionHeightInstructionMidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolutionScan] + 28, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "DC 0D ?? ?? ?? ?? DB 05 ?? ?? ?? ?? DE F9 D9 95 34 FF FF FF 51 D9 1C 24 8B 55 BC");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			dNewAspectRatio2 = (0.7 / (double)fAspectRatioScale) / (double)fAspectRatioScale;

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 04 95 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DC 35 ?? ?? ?? ?? D9 95 30 FF FF FF 51 D9 1C 24 8D 85 7C FF FF FF");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			CameraFOVOffset = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionScanResult + 3, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90", 7);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
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