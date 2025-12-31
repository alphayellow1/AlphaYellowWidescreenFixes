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
std::string sFixName = "FastLanesBowlingWidescreenFix";
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
float fNewAspectRatio4;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
float fNewCameraFOV5;

// Game detection
enum class Game
{
	FLB,
	Unknown
};

enum ResolutionInstructionsScansIndices
{
	RendererResolutionScan,
	Resolution2Scan,
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan,
	Resolution6Scan
};

enum AspectRatioInstructionsScansIndices
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan,
	AspectRatio4Scan
};

enum CameraFOVInstructionsScansIndices
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan,
	CameraFOV4Scan,
	CameraFOV5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FLB, {"Fast Lanes Bowling", "pcbowl.exe"}},
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

static SafetyHookMid RendererResolutionInstructionsHook{};
static SafetyHookMid ResolutionInstructions2Hook{};
static SafetyHookMid ResolutionInstructions3Hook{};
static SafetyHookMid ResolutionInstructions5Hook{};
static SafetyHookMid ResolutionWidth6Hook{};
static SafetyHookMid ResolutionHeight6Hook{};

void WidescreenFix()
{
	if (bFixActive == true && eGameType == Game::FLB)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB", "8B 48 ?? 8B 50 ?? 89 2D", "8B 7C 24 ?? 8B 6C 24 ?? 89 3D", "3B F9 0F 85 ?? ?? ?? ?? A1", "89 3D ?? ?? ?? ?? 89 2D ?? ?? ?? ?? EB", "8B 14 ?? 8B 8C 24");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution6Scan] - (std::uint8_t*)exeModule);

			// Renderer Resolution
			RendererResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			// Resolution 2
			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution3Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esp + 0x10) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esp + 0x14) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan], "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan] + 13, "\x90\x90\x90\x90", 4);

			ResolutionInstructions5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution6Scan], "\x90\x90\x90", 3);

			ResolutionWidth6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution6Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution6Scan] + 18, "\x90\x90\x90\x90", 4);

			ResolutionHeight6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution6Scan] + 18, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		
		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50", "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 11");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio4Scan] - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio1Scan] + 1, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio2Scan] + 1, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio3Scan] + 1, fNewAspectRatio);

			fNewAspectRatio4 = 0.4f * fAspectRatioScale;

			Memory::Write(AspectRatioInstructionsScansResult[AspectRatio4Scan] + 4, fNewAspectRatio4);
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24", "68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6", "68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50", "68 ?? ?? ?? ?? 51 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B 15", "68 ?? ?? ?? ?? 51 50 E8 ?? ?? ?? ?? 83 C4 ?? 39 74 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV5Scan] - (std::uint8_t*)exeModule);

			fNewCameraFOV1 = 0.4f * fAspectRatioScale;

			fNewCameraFOV2 = 0.4f * fAspectRatioScale * fFOVFactor;

			fNewCameraFOV3 = 0.4f * fAspectRatioScale;

			fNewCameraFOV4 = 0.4f * fAspectRatioScale;

			fNewCameraFOV5 = 0.4f * fAspectRatioScale;

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV1Scan] + 1, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV2Scan] + 1, fNewCameraFOV2);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV3Scan] + 1, fNewCameraFOV3);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV4Scan] + 1, fNewCameraFOV4);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV5Scan] + 1, fNewCameraFOV5);
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