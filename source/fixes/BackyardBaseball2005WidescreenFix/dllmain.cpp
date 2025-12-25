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
std::string sFixName = "BackyardBaseball2005WidescreenFix";
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
double dNewCameraFOV;
float fNewHUDHorizontalRes;

// Game detection
enum class Game
{
	BB2005_DX9,
	BB2005_DX9_PIXO,
	Unknown
};

enum ResolutionInstructionsIndices
{
	RendererResolutionScan,
	ViewportResolution1Scan,
	ViewportResolution2Scan,
	ViewportResolution3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BB2005_DX9, {"Backyard Baseball 2005 (DX9)", "Baseball.exe"}},
	{Game::BB2005_DX9_PIXO, {"Backyard Baseball 2005 (DX9 PIXOMATIC)", "BaseballSR.exe"}},
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

static SafetyHookMid ResolutionInstructions1Hook{};
static SafetyHookMid ViewportResolutionInstructions1Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction3Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction3Hook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid HUDHorizontalResInstructionHook{};

void WidescreenFix()
{
	if ((eGameType == Game::BB2005_DX9 || eGameType == Game::BB2005_DX9_PIXO) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 08 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? C7 05", "8B 48 ?? 8B 50 ?? 89 2D", "8B 4A ?? 89 0D ?? ?? ?? ?? 8B 56", "89 46 ?? 8B 3D");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution2Scan] - (std::uint8_t*)exeModule);

			ResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.eax + 0x4) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ViewportResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution2Scan], "\x90\x90\x90", 3);

			ViewportResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 15, "\x90\x90\x90", 3);

			ViewportResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 15, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution3Scan], "\x90\x90\x90", 3);

			ViewportResolutionWidthInstruction3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution3Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[ViewportResolution3Scan] + 33, "\x90\x90\x90", 3);

			ViewportResolutionHeightInstruction3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution3Scan] + 33, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x10) = iCurrentResY;
			});
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? 50 51 8B 0E D9 1C ?? E8 ?? ?? ?? ?? 8B 4C 24 ?? 89 4E ?? 5E");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(fNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "DC 0D ?? ?? ?? ?? D9 5C 24 08 8B 44 24 08 D9 44 24 08");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			dNewCameraFOV = 0.75 * (double)fFOVFactor;

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(dNewCameraFOV);
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