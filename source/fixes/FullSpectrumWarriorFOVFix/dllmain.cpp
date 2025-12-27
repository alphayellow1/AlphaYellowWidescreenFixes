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
std::string sFixName = "FullSpectrumWarriorFOVFix";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
std::string sDllName;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
std::uint8_t* AspectRatioInstructionScanResult;
std::vector<std::uint8_t*> CameraFOVInstructionsScansResult;
std::uint8_t* g_AspectRatioAddr = nullptr;
std::uint8_t* g_CameraFOV1Addr = nullptr;
std::uint8_t* g_CameraFOV2Addr = nullptr;

// Game detection
enum class Game
{
	FSW,
	Unknown
};

enum CameraFOVInstructionsIndex
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FSW, {"Full Spectrum Warrior", "Launcher.exe"}},
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

	dllModule2 = Memory::GetHandle("FSW.dll");

	sDllName = Memory::GetModuleName(dllModule2);

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};

static std::array<uint8_t, 6> g_AspectRatioHookBytes{};
static std::array<uint8_t, 6> g_CameraFOV1HookBytes{};
static std::array<uint8_t, 6> g_CameraFOV2HookBytes{};

void PatchAspectRatio()
{
	memcpy(g_AspectRatioHookBytes.data(), AspectRatioInstructionScanResult, g_AspectRatioHookBytes.size());

	Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

	AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
	{
		FPU::FLD(fNewAspectRatio);
	});

	memcpy(g_AspectRatioHookBytes.data(), AspectRatioInstructionScanResult, g_AspectRatioHookBytes.size());

	g_AspectRatioAddr = AspectRatioInstructionScanResult;

	FlushInstructionCache(GetCurrentProcess(), g_AspectRatioAddr, g_AspectRatioHookBytes.size());
}

void PatchFOV1()
{
	memcpy(g_CameraFOV1HookBytes.data(), CameraFOVInstructionsScansResult[CameraFOV1Scan], g_CameraFOV1HookBytes.size());

	Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

	CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.ebx + 0xF0);

		if (fCurrentCameraFOV1 != fNewCameraFOV1)
		{
			fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, fAspectRatioScale) * fFOVFactor;
		}			

		FPU::FLD(fNewCameraFOV1);
	});

	memcpy(g_CameraFOV1HookBytes.data(), CameraFOVInstructionsScansResult[CameraFOV1Scan], g_CameraFOV1HookBytes.size());

	g_CameraFOV1Addr = CameraFOVInstructionsScansResult[CameraFOV1Scan];

	FlushInstructionCache(GetCurrentProcess(), g_CameraFOV1Addr, g_CameraFOV1HookBytes.size());
}

void PatchFOV2()
{
	memcpy(g_CameraFOV2HookBytes.data(), CameraFOVInstructionsScansResult[CameraFOV2Scan], g_CameraFOV2HookBytes.size());

	Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

	CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.eax + 0xF0);

		if (fCurrentCameraFOV2 != fNewCameraFOV2)
		{
			fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale) * fFOVFactor;
		}		

		ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV2);
	});

	memcpy(g_CameraFOV2HookBytes.data(), CameraFOVInstructionsScansResult[CameraFOV2Scan], g_CameraFOV2HookBytes.size());

	g_CameraFOV2Addr = CameraFOVInstructionsScansResult[CameraFOV2Scan];

	FlushInstructionCache(GetCurrentProcess(), g_CameraFOV2Addr, g_CameraFOV2HookBytes.size());
}

void FOVFix()
{
	if (eGameType == Game::FSW && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
		
		AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC C7 01");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {}+{:x}", sDllName, AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			g_AspectRatioAddr = AspectRatioInstructionScanResult;

			PatchAspectRatio();
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}
		
		CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 83 ?? ?? ?? ?? 8B 03", "8B 88 ?? ?? ?? ?? 8B 93 ?? ?? ?? ?? 83 C2");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {}+{:x}", sDllName, CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 2: Address is {}+{:x}", sDllName, CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)dllModule2);

			g_CameraFOV1Addr = CameraFOVInstructionsScansResult[CameraFOV1Scan];

			static std::array<uint8_t, 6> g_OriginalFOV1Bytes{};
			memcpy(g_OriginalFOV1Bytes.data(), g_CameraFOV1Addr, 6);

			PatchFOV1();

			g_CameraFOV2Addr = CameraFOVInstructionsScansResult[CameraFOV2Scan];

			static std::array<uint8_t, 6> g_OriginalFOV2Bytes{};
			memcpy(g_OriginalFOV2Bytes.data(), g_CameraFOV2Addr, 6);

			PatchFOV2();
		}
	}
}

DWORD WINAPI PatchWatchdogThread(LPVOID)
{
    while (true)
    {
		Sleep(500);

		dllModule2 = Memory::GetHandle("FSW.dll", 200, 0, false);

		g_AspectRatioAddr = (std::uint8_t*)dllModule2 + 0x465990;

		g_CameraFOV1Addr = (std::uint8_t*)dllModule2 + 0x40139C;

		g_CameraFOV2Addr = (std::uint8_t*)dllModule2 + 0x167F57;

        if (g_AspectRatioAddr)
        {
            if (memcmp(g_AspectRatioAddr, g_AspectRatioHookBytes.data(), g_AspectRatioHookBytes.size()) != 0)
            {
				Memory::PatchBytes(g_AspectRatioAddr, reinterpret_cast<const char*>(g_AspectRatioHookBytes.data()), g_AspectRatioHookBytes.size());
            }
        }

        if (g_CameraFOV1Addr)
        {
            if (memcmp(g_CameraFOV1Addr, g_CameraFOV1HookBytes.data(), g_CameraFOV1HookBytes.size()) != 0)
            {
				Memory::PatchBytes(g_CameraFOV1Addr, reinterpret_cast<const char*>(g_CameraFOV1HookBytes.data()), g_CameraFOV1HookBytes.size());
            }
        }

        if (g_CameraFOV2Addr)
        {
            if (memcmp(g_CameraFOV2Addr, g_CameraFOV2HookBytes.data(), g_CameraFOV2HookBytes.size()) != 0)
            {
				Memory::PatchBytes(g_CameraFOV2Addr, reinterpret_cast<const char*>(g_CameraFOV2HookBytes.data()), g_CameraFOV2HookBytes.size());
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
		FOVFix();

		CreateThread(nullptr, 0, PatchWatchdogThread, nullptr, 0, nullptr);
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