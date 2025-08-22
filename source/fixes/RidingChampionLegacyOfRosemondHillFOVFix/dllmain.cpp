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
#include <tlhelp32.h>             // For CreateToolhelp32Snapshot, MODULEENTRY32
#include <psapi.h>                // For GetModuleInformation
#include <filesystem>
#include <fstream>
#include <cmath>                  // For atanf, tanf
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = NULL;    // will become game.exe’s base
HMODULE thisModule;
HANDLE monitoringThread = NULL;
volatile BOOL stopMonitoring = FALSE;
CRITICAL_SECTION cs;

// Fix details
std::string sFixName = "RidingChampionLegacyOfRosemondHillFOVFix";
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

// Variables
int iCurrentResX;
int iCurrentResY;
float fAspectRatioScale;
float fFOVFactor;
float fNewCameraVFOV;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
static uint8_t* CameraFOV1Address;
static uint8_t* CameraFOV2Address;
static uint8_t* CameraFOV3Address;

// Game detection
enum class Game
{
    RCLORH,
    Unknown
};

struct GameInfo
{
    std::string GameTitle;
    std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
    {Game::RCLORH, {"Riding Champion: Legacy of Rosemond Hill", "game.exe"}},
};

const GameInfo* game = nullptr;
Game eGameType = Game::Unknown;

// Utility: enumerate modules via Toolhelp to find game.exe
HMODULE findGameModule()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE)
        return NULL;

    MODULEENTRY32W me;
    me.dwSize = sizeof(me);

    if (Module32FirstW(snap, &me))
    {
        do
        {
            // wide-char case-insensitive compare
            if (_wcsicmp(me.szModule, L"game.exe") == 0)
            {
                CloseHandle(snap);
                return (HMODULE)me.modBaseAddr;
            }
        } while (Module32NextW(snap, &me));
    }

    CloseHandle(snap);
    return NULL;
}

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
        spdlog::set_level(spdlog::level::debug);

        spdlog::info("----------");
        spdlog::info("{:s} v{:s} loaded.", sFixName.c_str(), sFixVersion.c_str());
        spdlog::info("Log file: {}", sExePath.string() + "\\" + sLogFile);
        spdlog::info("Module Name: {0:s}", sExeName.c_str());
        spdlog::info("Module Path: {0:s}", sExePath.string());
        spdlog::info("Module Address: 0x{0:X}", (uintptr_t)exeModule);
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
    std::ifstream iniFile(sFixPath.string() + "\\" + sConfigFile);
    if (!iniFile)
    {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        std::cout << sFixName << " v" << sFixVersion << " loaded." << std::endl;
        std::cout << "ERROR: Could not locate config file." << std::endl;
        spdlog::shutdown();
        FreeLibraryAndExitThread(thisModule, 1);
    }
    else
    {
        spdlog::info("Config file: {}", sFixPath.string() + "\\" + sConfigFile);
        ini.parse(iniFile);
    }

    ini.strip_trailing_comments();
    spdlog::info("----------");

    inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
    spdlog_confparse(bFixActive);

    inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
    inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
    inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
    spdlog_confparse(iCurrentResX);
    spdlog_confparse(iCurrentResY);
    spdlog_confparse(fFOVFactor);

    if (iCurrentResX <= 0 || iCurrentResY <= 0)
    {
        spdlog::info("Resolution not specified; using desktop resolution.");
        auto dims = Util::GetPhysicalDesktopDimensions();
        iCurrentResX = dims.first;
        iCurrentResY = dims.second;
        spdlog_confparse(iCurrentResX);
        spdlog_confparse(iCurrentResY);
    }

    spdlog::info("----------");
}

bool DetectGame()
{
    for (const auto& [type, info] : kGames)
    {
        // _stricmp returns 0 if the two C-strings match, ignoring case :contentReference[oaicite:1]{index=1}
        if (_stricmp(info.ExeName.c_str(), sExeName.c_str()) == 0)
        {
            spdlog::info("Detected game: {} ({})", info.GameTitle, sExeName);
            eGameType = type;
            game = &info;
            return true;
        }
    }

    spdlog::error("Unsupported game: {}.", sExeName);
    return false;
}

static SafetyHookMid CameraVFOVInstruction1Hook{};

void CameraVFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewCameraVFOV]
	}
}

static SafetyHookMid CameraVFOVInstruction2Hook{};

void CameraVFOVInstruction2MidHook(SafetyHookContext& ctx)
{
    _asm
    {
        fld dword ptr ds:[fNewCameraVFOV]
    }
}

static SafetyHookMid CameraFOVInstruction1Hook{};

void CameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
    float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(CameraFOV1Address);

    fNewCameraFOV1 = fCurrentCameraFOV1 * (1.0f / fFOVFactor);

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV1]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
    float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(CameraFOV2Address);

    fNewCameraFOV2 = fCurrentCameraFOV2 * (1.0f / fFOVFactor);

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
    float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(CameraFOV3Address);

    fNewCameraFOV3 = fCurrentCameraFOV3 * (1.0f / fFOVFactor);

	_asm
	{
		fmul dword ptr ds:[fNewCameraFOV3]
	}
}

void FOVFix()
{
	if (eGameType == Game::RCLORH && bFixActive == true)
	{
        static bool bCameraVFOV1Done = false;
        
        static bool bCameraVFOV2Done = false;
        
        static bool bCameraFOV1Done = false;
       
        static bool bCameraFOV2Done = false;

		static bool bCameraFOV3Done = false;

        fNewCameraVFOV = 1.0f;

        if (bCameraVFOV1Done == false)
        {
            std::uint8_t* CameraVFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 05 58 F8 51 00 D8 4A 14 D8 C9 D8 44 24 0C D9 5A 20 DD D8 85 F6");
            if (CameraVFOVInstruction1ScanResult)
            {
                spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

                Memory::PatchBytes(CameraVFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

                CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraVFOVInstruction1ScanResult, CameraVFOVInstruction1MidHook);

                bCameraVFOV1Done = true;
            }
            else
            {
                spdlog::error("Failed to locate camera VFOV instruction 1 memory address.");
                return;
            }
        }

        if (bCameraVFOV2Done == false)
        {
            std::uint8_t* CameraVFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 05 58 F8 51 00 D8 4C 24 00 D9 5C 24 08 7E 25 8D 42 1C D9 44 24 00 D8 48 F4 83 C0 28 49");
            if (CameraVFOVInstruction2ScanResult)
            {
                spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
                
                Memory::PatchBytes(CameraVFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
                
                CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraVFOVInstruction2ScanResult, CameraVFOVInstruction2MidHook);
                
                bCameraVFOV2Done = true;
            }
            else
            {
                spdlog::error("Failed to locate camera VFOV instruction 2 memory address.");
                return;
            }
        }

        if (bCameraFOV1Done == false)
        {
            std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 05 88 FC 51 00 D8 0D 14 D2 49 00 A1 50 F8 51 00 8B 15 54 F8 51 00 D1 F8");
            if (CameraFOVInstruction1ScanResult)
            {
                spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

                CameraFOV1Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction1ScanResult + 2, Memory::PointerMode::Absolute);
                
                Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

                CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, CameraFOVInstruction1MidHook);

                bCameraFOV1Done = true;
            }
            else
            {
                spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
                return;
            }
        }
        
        if (bCameraFOV2Done == false)
        {
            std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "D9 05 88 FC 51 00 D8 0D 14 D2 49 00 D8 35 18 D2 49 00 D9 5C 24 00");
            if (CameraFOVInstruction2ScanResult)
            {
                spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult - (std::uint8_t*)exeModule);
                
                CameraFOV2Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction2ScanResult + 2, Memory::PointerMode::Absolute);

                Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
                
                CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, CameraFOVInstruction2MidHook);

			    bCameraFOV2Done = true;
            }
            else
            {
                spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
                return;
            }
        }

        if (bCameraFOV3Done == false)
        {
			std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(exeModule, "D8 0D 88 FC 51 00 D9 54 24 08 D9 05 58 F8 51 00 DC 1D 68 73 48 00 DF E0 F6 C4 44 7B");
            if (CameraFOVInstruction3ScanResult)
            {
                spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction3ScanResult - (std::uint8_t*)exeModule);

                CameraFOV3Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction3ScanResult + 2, Memory::PointerMode::Absolute);

                Memory::PatchBytes(CameraFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

                CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult, CameraFOVInstruction3MidHook);

                bCameraFOV3Done = true;
            }
			else
			{
				spdlog::error("Failed to locate camera FOV instruction 3 memory address.");
				return;
			}
        }
	}
}

DWORD __stdcall Main(void*)
{
	if (DetectGame())
		FOVFix();
	return TRUE;
}

DWORD WINAPI MonitorThread(LPVOID)
{
    bool didInit = false;
    bool didDetect = false;

    while (!stopMonitoring)
    {
        HMODULE gm = findGameModule();
        if (gm)
        {
            if (!didInit)
            {
                exeModule = gm;
                Logging();
                Configuration();
                didInit = true;
            }

            if (!didDetect)
            {
                didDetect = DetectGame();   // only call once
            }

            if (didDetect)
            {
                EnterCriticalSection(&cs);
                FOVFix();                   // now applies only when game.exe matched
                LeaveCriticalSection(&cs);
            }
        }
        Sleep(1000);
    }
    return 0;
}

extern "C" __declspec(dllexport) void StartMonitoring()
{
    if (!monitoringThread)
    {
        stopMonitoring = FALSE;
        InitializeCriticalSection(&cs);
        monitoringThread = CreateThread(NULL, 0, MonitorThread, NULL, 0, NULL);
        if (monitoringThread)
        {
            SetThreadPriority(monitoringThread, THREAD_PRIORITY_HIGHEST);
            CloseHandle(monitoringThread);
        }
    }
}

extern "C" __declspec(dllexport) void StopMonitoring()
{
	if (monitoringThread)
	{
		stopMonitoring = TRUE;
		WaitForSingleObject(monitoringThread, INFINITE);
		DeleteCriticalSection(&cs);
		monitoringThread = NULL;
	}
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        thisModule = hMod;
        StartMonitoring();  // no more direct Main() here
    }
    return TRUE;
}