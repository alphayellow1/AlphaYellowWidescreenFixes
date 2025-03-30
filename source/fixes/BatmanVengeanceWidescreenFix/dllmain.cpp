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
HMODULE dllModule3 = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "BatmanVengeanceWidescreenFix";
std::string sFixVersion = "1.4";
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
constexpr float fTolerance = 0.0001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraVFOV;
float fNewAspectRatio;
float fFOVFactor;
float fModifiedFOVValue;

// Game detection
enum class Game
{
	BV,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BV, {"Batman Vengeance", "Batman.exe"}},
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
		spdlog::info("Module Address: 0x{0:X}", (uintptr_t)dllModule);
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
		}
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}

	while ((dllModule2 = GetModuleHandleA("osr_dx8_vf.dll")) == nullptr)
	{
		spdlog::warn("osr_dx8_vf.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for osr_dx8_vf.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while ((dllModule3 = GetModuleHandleA("enginecore_vf.dll")) == nullptr)
	{
		spdlog::warn("enginecore_vf.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for enginecore_vf.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	return true;
}

static SafetyHookMid CameraVFOVInstructionHook{};

void CameraVFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fNewCameraVFOV = fNewAspectRatio / fOldAspectRatio;

	_asm
	{
		fdivr dword ptr ds : [fNewCameraVFOV]
	}
}

float CalculateNewFOV(float fCurrentFOV)
{
	return fCurrentFOV * (fNewAspectRatio / fOldAspectRatio);
}

void WidescreenFix()
{
	if (eGameType == Game::BV && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionScanResult = Memory::PatternScan(dllModule2, "00 00 00 00 02 00 00 80 01 00 00 80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 00 05 00 00 00 04 00 00 40 06 00 00 B0 04 00 00 3C 00 00 00");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Width: Address is osr_dx8_vf.dll+{:x}", ResolutionScanResult + 4 - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Height: Address is osr_dx8_vf.dll+{:x}", ResolutionScanResult + 8 - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionScanResult + 11, iCurrentResX);

			Memory::Write(ResolutionScanResult + 15, iCurrentResY);

			Memory::Write(ResolutionScanResult + 19, iCurrentResX);

			Memory::Write(ResolutionScanResult + 23, iCurrentResY);

			Memory::Write(ResolutionScanResult + 27, iCurrentResX);

			Memory::Write(ResolutionScanResult + 31, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D8 3D ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D9 C2 D8 4D 1C D8 C9 D9 18 D9 EE D9 58 04 D9 EE D9 58 08 D9 EE D9 58 0C D9 EE D9 58 10 D9 C1 D8 4D 1C D8 C9 D9 58 14 DD D8 D9 EE D9 58 18 D9 EE D9 58 1C D9 45 0C D8 45 10 D8 CA D9 58 20 D9 45 14 D8 45 18 D8 C9 D9 58 24 DD D8 DD D8 D9 45 1C D8 65 20 D8 7D 20 D9 50 28 D9 EE D9 58 30 D9 EE D9 58 34 D8 4D 1C D9 58 38");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is osr_dx8_vf.dll+{:x}", CameraVFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraVFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraVFOVInstructionScanResult + 6, CameraVFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera VFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule3, "D8 4C 24 0C D9 5C 24 20 D9 44 24 24 D8 74 24 28 8B 54 24 20 D8 4C 24 20 D9 5C 24 24");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is enginecore_vr.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule3);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			static float fLastModifiedFOV = 0.0f;

			static std::vector<float> vComputedFOVs;

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0xC);

				// Skip processing if a similar HFOV (within tolerance) has already been computed
				bool alreadyComputed = std::any_of(vComputedFOVs.begin(), vComputedFOVs.end(),
					[&](float computedValue) {
					return std::fabs(computedValue - fCurrentCameraFOV) < fTolerance;
				});

				if (alreadyComputed)
				{
					return;
				}

				fCurrentCameraFOV = CalculateNewFOV(fCurrentCameraFOV) * fFOVFactor;

				// Stores the new value so future calls can skip re-calculations
				vComputedFOVs.push_back(fCurrentCameraFOV);
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
