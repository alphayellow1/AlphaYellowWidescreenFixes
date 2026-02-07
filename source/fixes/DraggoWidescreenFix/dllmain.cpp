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
HMODULE dllModule2 = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "DraggoWidescreenFix";
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
int iNewAspectRatio;
int iNewCameraFOV;

// Game detection
enum class Game
{
	DRAGGO,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1Scan,
	Res2Scan,
	Res3Scan,
	Res4Scan,
	Res5Scan,
	Res6Scan,
	Res7Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DRAGGO, {"Draggo", "Draggo.exe"}},
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

static SafetyHookMid ResolutionWidthInstruction1Hook{};
static SafetyHookMid ResolutionHeightInstruction1Hook{};
static SafetyHookMid ResolutionInstructions2Hook{};
static SafetyHookMid ResolutionInstructions3Hook{};
static SafetyHookMid ResolutionWidthInstruction4Hook{};
static SafetyHookMid ResolutionHeightInstruction4Hook{};
static SafetyHookMid ResolutionWidthInstruction5Hook{};
static SafetyHookMid ResolutionHeightInstruction5Hook{};
static SafetyHookMid ResolutionWidthInstruction6Hook{};
static SafetyHookMid ResolutionHeightInstruction6Hook{};
static SafetyHookMid ResolutionWidthInstruction7Hook{};
static SafetyHookMid ResolutionHeightInstruction7Hook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::DRAGGO && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "0F BF 46 ?? A3 ?? ?? ?? ?? A3", "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 3D", "8B 54 24 ?? 8B 4C 24 ?? A8", "8B 35 ?? ?? ?? ?? 33 D2",
		"8B 3D ?? ?? ?? ?? C1 FA", "8B 2D ?? ?? ?? ?? 56 8B 35 ?? ?? ?? ?? 8D 41", "0F BF 46 ?? A3 ?? ?? ?? ?? 83 FD");

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "DB 40 ?? A1 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 83 38");

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "DB 40 ?? DC 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? DB 40 ?? A1 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 83 38");

		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res6Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res7Scan] - (std::uint8_t*)exeModule);

			ResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int16_t*>(ctx.esi + 0x4) = iCurrentResX;
			});

			ResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res1Scan] + 14, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int16_t*>(ctx.esi + 0x6) = iCurrentResY;
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res2Scan], 11);

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res2Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res3Scan], 8);

			ResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res3Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res4Scan], 6);

			ResolutionWidthInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4Scan], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res4Scan] + 98, 6);

			ResolutionHeightInstruction4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res4Scan] + 98, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res5Scan], 6);

			ResolutionWidthInstruction5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res5Scan], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res5Scan] + 107, 6);

			ResolutionHeightInstruction5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res5Scan] + 107, [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res6Scan] + 17, 6);

			ResolutionWidthInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res6Scan] + 17, [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Res6Scan], 6);

			ResolutionHeightInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res6Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ResolutionWidthInstruction7Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res7Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int16_t*>(ctx.esi + 0x4) = iCurrentResX;
			});

			ResolutionHeightInstruction7Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Res7Scan] + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int16_t*>(ctx.esi + 0x6) = iCurrentResY;
			});
		}
		
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(AspectRatioInstructionScanResult, 3);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentAspectRatio = *reinterpret_cast<int*>(ctx.eax + 0x4C);
				
				iNewAspectRatio = (int)(iCurrentAspectRatio * fAspectRatioScale);

				FPU::FILD(iNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}
		
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 3);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV = *reinterpret_cast<int*>(ctx.eax + 0x48);

				iNewCameraFOV = (int)(iCurrentCameraFOV * fAspectRatioScale * fFOVFactor);

				FPU::FILD(iNewCameraFOV);
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