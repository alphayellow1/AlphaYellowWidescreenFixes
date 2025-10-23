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
#include <unordered_set>
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
std::string sFixName = "CTUMarineSharpshooterFOVFix";
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
constexpr float fDefaultCameraHFOV = 1.0471975803375244f; // 60 degrees in radians
constexpr float fDefaultCameraHFOV2 = 1.046400427818f;
constexpr float fDefaultCameraVFOV = 0.8726646900177002f; // 50 degrees in radians
constexpr float fDefaultCameraVFOV2 = 0.871933996677f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;

// Game detection
enum class Game
{
	CTUMS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CTUMS, {"CTU Marine Sharpshooter", "lithtech.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("CSHELL.DLL")) == nullptr)
	{
		spdlog::warn("CSHELL.DLL not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for CSHELL.DLL: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::CTUMS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
		
		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 98 01 00 00 89 45 BC 8B 81 9C 01 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.ecx + 0x198);

				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.ecx + 0x19C);

				spdlog::info("[Hook] Raw incoming HFOV: {:.12f}", fCurrentCameraHFOV);

				if (fCurrentCameraHFOV != fNewCameraHFOV)
				{
					if ((Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV) || Maths::isClose(fCurrentCameraHFOV, fDefaultCameraHFOV2)) && (Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV) || Maths::isClose(fCurrentCameraVFOV, fDefaultCameraVFOV2)))
					{
						fNewCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, fAspectRatioScale, fFOVFactor);
					}
					else
					{
						fNewCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, fAspectRatioScale);
					}					
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionScanResult + 9, "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 9, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x198);

				float& fCurrentCameraVFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x19C);

				spdlog::info("[Hook] Raw incoming VFOV: {:.12f}", fCurrentCameraVFOV2);

				if (fCurrentCameraVFOV2 != fNewCameraVFOV)
				{
					if ((Maths::isClose(fCurrentCameraHFOV2, fDefaultCameraHFOV) || Maths::isClose(fCurrentCameraHFOV2, fDefaultCameraHFOV2)) && (Maths::isClose(fCurrentCameraVFOV2, fDefaultCameraVFOV) || Maths::isClose(fCurrentCameraVFOV2, fDefaultCameraVFOV2)))
					{
						fNewCameraVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentCameraVFOV2, fFOVFactor);
					}
					else
					{
						fNewCameraVFOV = fCurrentCameraVFOV2;
					}
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
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