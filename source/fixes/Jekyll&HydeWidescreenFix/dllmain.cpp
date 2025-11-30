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
HMODULE dllModule2;

// Fix details
std::string sFixName = "Jekyll&HydeWidescreenFix";
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
float fNewCameraFOV1;
float fNewCameraFOV2;

// Game detection
enum class Game
{
	JAH_GAME,
	JAH_VIDEOCONFIG,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AspectRatio1Scan,
	AspectRatio2Scan
};

enum CameraFOVInstructionsIndices
{
	CameraFOV1Scan,
	CameraFOV2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::JAH_GAME, {"Jekyll & Hyde", "Game.exe"}},
	{Game::JAH_VIDEOCONFIG, {"Jekyll & Hyde - Video Configuration", "video.exe"}}
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
	bool bGameFound = false;

	for (const auto& [type, info] : kGames)
	{
		if (Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			spdlog::info("Detected game: {:s} ({:s})", info.GameTitle, sExeName);
			spdlog::info("----------");
			eGameType = type;
			game = &info;
			bGameFound = true;
			break;
		}
	}

	if (bGameFound == false)
	{
		spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		return false;
	}

	dllModule2 = Memory::GetHandle("fnx_dx7.dll");

	return true;
}

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::JAH_VIDEOCONFIG)
		{
			std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "00 05 00 00 0F 85 0E 01 00 00 8B 16 8B CE FF 52 20 3D 00 04 00 00");
			if (ResolutionInstructionsScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);
				
				Memory::Write(ResolutionInstructionsScanResult, iCurrentResX);
				
				Memory::Write(ResolutionInstructionsScanResult + 18, iCurrentResY);
			}
			else
			{
				spdlog::info("Cannot locate the resolution instructions scan memory address.");
				return;
			}
		}

		if (eGameType == Game::JAH_GAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::vector<std::uint8_t*> AspectRatioInstructionsScanResult = Memory::PatternScan(dllModule2, "8B 8E ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 8B 86", "D8 B7");
			if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScanResult) == true)
			{
				spdlog::info("Aspect Ratio Instruction 1: Address is fnx_dx7.dll+{:x}", AspectRatioInstructionsScanResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Aspect Ratio Instruction 2: Address is fnx_dx7.dll+{:x}", AspectRatioInstructionsScanResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

				Memory::PatchBytes(AspectRatioInstructionsScanResult[AspectRatio1Scan], "\x90\x90\x90\x90\x90\x90", 6);

				AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScanResult[AspectRatio1Scan], [](SafetyHookContext& ctx)
				{
					ctx.ecx = std::bit_cast<uintptr_t>(fNewAspectRatio);
				});

				Memory::PatchBytes(AspectRatioInstructionsScanResult[AspectRatio2Scan], "\x90\x90\x90\x90\x90\x90", 6);

				AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScanResult[AspectRatio2Scan], [](SafetyHookContext& ctx)
				{
					FPU::FDIV(fNewAspectRatio);
				});
			}

			std::vector<std::uint8_t*> CameraFOVInstructionsScanResult = Memory::PatternScan(dllModule2, "8B 8E ?? ?? ?? ?? 52", "D9 87 ?? ?? ?? ?? 8D 4C 24");
			if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScanResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is fnx_dx7.dll+{:x}", CameraFOVInstructionsScanResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Camera FOV Instruction 2: Address is fnx_dx7.dll+{:x}", CameraFOVInstructionsScanResult[CameraFOV2Scan] - (std::uint8_t*)exeModule);

				Memory::PatchBytes(CameraFOVInstructionsScanResult[CameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

				CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScanResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.esi + 0xCC);

					spdlog::info("[Hook] Raw incoming FOV 1: {:.12f}", fCurrentCameraFOV1);

					fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, fAspectRatioScale) * fFOVFactor;

					ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV1);
				});

				Memory::PatchBytes(CameraFOVInstructionsScanResult[CameraFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

				CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScanResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.edi + 0xCC);

					spdlog::info("[Hook] Raw incoming FOV 2: {:.12f}", fCurrentCameraFOV2);

					fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale) * fFOVFactor;

					FPU::FLD(fNewCameraFOV2);
				});
			}
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