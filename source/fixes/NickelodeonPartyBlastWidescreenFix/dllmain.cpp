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
HMODULE thisModule;

// Fix details
std::string sFixName = "NickelodeonPartyBlastWidescreenFix";
std::string sFixVersion = "1.3";
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
float fNewAspectRatio2;
float fNewCameraFOV;
int iNewHorizontalRes;

// Game detection
enum class Game
{
	NPB,
	Unknown
};

enum ResolutionInstructionsIndex
{
	RendererResolutionScan,
	ViewportResolution1Scan,
	ViewportResolution2Scan,
	ViewportResolution3Scan,
	ViewportResolution4Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::NPB, {"Nickelodeon Party Blast", "nGames_PC.exe"}},
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

static SafetyHookMid RendererResolutionInstructionsHook{};
static SafetyHookMid ViewportResolutionWidthInstruction1Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction1Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction3Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction3Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction4Hook{};
static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::NPB && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB", "8B 4D ?? A1 ?? ?? ?? ?? 89 4C 24", "8B 4E ?? 89 54 24 ?? 8B 56 ?? 89 44 24 ?? A1", "DB 41 ?? 56", "DB 05 ?? ?? ?? ?? C7 44 24 5C 00 00 00 00 C7 44 24 60 00 00 00 3F C7 44 24 6C 00 00 00 00 C7 44 24 70 00 00 00 3F D8 0D ?? ?? ?? ?? C7 44 24 7C 00 00 00 00 C7 84 24 80 00 00 00 00 00 00 3F C7 84 24 8C 00 00 00 00 00 00 00 C7 84 24 90 00 00 00 00 00 00 3F D8 08 C7 84 24 9C 00 00 00 00 00 00 00 C7 84 24 A0 00 00 00 00 00 00 3F C7 84 24 AC 00 00 00 00 00 00 00 C7 84 24 B0 00 00 00 00 00 00 3F 8D 6C 24 58 8D B4 24 B4 00 00 00 D9 18 DB 05 ?? ?? ?? ?? 8B 08 89 4C 24 74 89 8C 24 84 00 00 00 D8 0D ?? ?? ?? ?? D8 48 08 D9 58 08 DB 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 48 04 D9 58 04 DB 05 ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResolutionScan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportResolution4Scan] - (std::uint8_t*)exeModule);

			RendererResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResolutionScan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportResolution1Scan], 3);

			ViewportResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 12, 3);

			ViewportResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution1Scan] + 12, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportResolution2Scan], 3);

			ViewportResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 7, 3);

			ViewportResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution2Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportResolution3Scan], 3);

			iNewHorizontalRes = (int)(640.0f * fAspectRatioScale);

			ViewportResolutionWidthInstruction3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportResolution3Scan], [](SafetyHookContext& ctx)
			{
				FPU::FILD(iNewHorizontalRes);
			});

			int* iNewHorizontalResPtr = &iCurrentResX;

			int* iNewVerticalResPtr = &iCurrentResY;

			Memory::Write(ResolutionInstructionsScansResult[ViewportResolution4Scan] + 2, iNewHorizontalResPtr);

			Memory::Write(ResolutionInstructionsScansResult[ViewportResolution4Scan] + 146, iNewHorizontalResPtr);

			Memory::Write(ResolutionInstructionsScansResult[ViewportResolution4Scan] + 177, iNewVerticalResPtr);

			Memory::Write(ResolutionInstructionsScansResult[ViewportResolution4Scan] + 195, iNewVerticalResPtr);
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D8 70 ?? DB 05");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 3);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.eax + 0x14);

				fNewAspectRatio2 = fCurrentAspectRatio / fAspectRatioScale;

				FPU::FDIV(fNewAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 40 18 D9 F2 66 8B 48 10 66 8B 50 12 89 4C 24 0C 8D 4C 24 00 51");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 3);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.eax + 0x18);

				if (fCurrentCameraFOV == 0.6981317401f)
				{
					fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;
				}
				else
				{
					fNewCameraFOV = fCurrentCameraFOV;
				}

				FPU::FLD(fNewCameraFOV);
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