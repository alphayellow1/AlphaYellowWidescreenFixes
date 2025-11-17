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
HMODULE dllModule2 = nullptr;

// Fix details
std::string sFixName = "TheHistoryChannelCivilWarANationDividedFOVFix";
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
float fNewCameraFOV2;
float fNewZoomCameraFOV;
float fZoomFOVFactor;
uint8_t* CameraFOV2Address;

// Game detection
enum class Game
{
	THCCWAND,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::THCCWAND, {"The History Channel: Civil War - A Nation Divided", "cw.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load resolution from ini
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	inipp::get_value(ini.sections["Settings"], "ZoomFOVFactor", fZoomFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fZoomFOVFactor);

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

	while ((dllModule2 = GetModuleHandleA("CloakNTEngine.dll")) == nullptr)
	{
		spdlog::warn("CloakNTEngine.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for CloakNTEngine.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid ZoomCameraFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::THCCWAND && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 96 B0 00 00 00 89 54 24 08 8A 8E BE 00 00 00 84 C9");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);			

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0xB0);

				if (fCurrentCameraFOV == 1.220000029f)
				{
					fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale);
				}

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule2, "8B 0D ?? ?? ?? ?? 89 86 78 02 00 00 8B D1 89 88 AC 00 00 00 89 90 B0 00 00 00");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is CloakNTEngine.dll+{:x}", CameraFOVInstruction2ScanResult - (std::uint8_t*)dllModule2);
			
			CameraFOV2Address = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstruction2ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);			
			
			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(CameraFOV2Address);

				fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV2);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* ZoomCameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 49 08 89 0C 10 8B 45 D0 83 C0 0C 89 45 D0 8B 00");
		if (ZoomCameraFOVInstructionScanResult)
		{
			spdlog::info("Zoom Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", ZoomCameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);
			
			Memory::PatchBytes(ZoomCameraFOVInstructionScanResult, "\x90\x90\x90", 3);
			
			ZoomCameraFOVInstructionHook = safetyhook::create_mid(ZoomCameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomCameraFOV = *reinterpret_cast<float*>(ctx.ecx + 0x8);				
				
				fNewZoomCameraFOV = fCurrentZoomCameraFOV / fZoomFOVFactor;
				
				ctx.ecx = std::bit_cast<uintptr_t>(fNewZoomCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate zoom camera FOV instruction memory address.");
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
		FOVFix();
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