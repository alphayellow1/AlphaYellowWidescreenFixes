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
#include <atomic>
#include <thread>
#include <mutex>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "GSectorWidescreenFix";
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
float fNewCameraFOV;
uint8_t* ResolutionWidth3Address;
uint8_t* ResolutionHeight3Address;
uint8_t* ResolutionWidth4Address;
uint8_t* ResolutionHeight4Address;
uint8_t* ResolutionWidth5Address;
uint8_t* ResolutionHeight5Address;

std::atomic<bool> bD3DPatched{ false };
std::atomic<HMODULE> g_lastD3DModule{ nullptr };
std::mutex g_patchMutex;

// Game detection
enum class Game
{
	GS,
	Unknown
};

enum ResolutionInstructions1Index
{
	Resolution1Scan,
	Resolution2Scan
};

enum ResolutionInstructions2Index
{
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::GS, {"G-Sector", "g-sector.exe"}},
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

static DWORD __stdcall D3DWatcherThread(void*);

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
	
	DWORD watcherThreadId = 0;

	HANDLE hWatcher = CreateThread(nullptr, 0, D3DWatcherThread, nullptr, 0, &watcherThreadId);
	if (hWatcher)
	{
		SetThreadPriority(hWatcher, THREAD_PRIORITY_NORMAL);
		CloseHandle(hWatcher);
	}

	return true;
}

void WidescreenFix()
{
	if (eGameType == Game::GS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "C7 05 ?? ?? ?? ?? 20 03 00 00 C7 05 ?? ?? ?? ?? 58 02 00 00 E8 ?? ?? ?? ?? 83 C4 0C 85 C0 75 1A", "C7 05 ?? ?? ?? ?? 80 02 00 00 C7 05 ?? ?? ?? ?? E0 01 00 00 C7 05 ?? ?? ?? ?? 03 00 00 00 83 C4 08 C3 C7 05 ?? ?? ?? ?? 9A 99 99 3E C7 05 ?? ?? ?? ?? 20 03 00 00 C7 05 ?? ?? ?? ?? 58 02 00 00 C7 05 ?? ?? ?? ?? 02 00 00 00 83 C4 08 C3 C7 05 ?? ?? ?? ?? 9A 99 99 3E C7 05 ?? ?? ?? ?? 00 04 00 00 C7 05 ?? ?? ?? ?? 00 03 00 00");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 50, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 60, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 94, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 104, iCurrentResY);
		}
	}
}

void ApplyD3DFix(HMODULE dllModule)
{
	std::lock_guard<std::mutex> lock(g_patchMutex);

	if (bD3DPatched.load())
	{
		// Already applied � skip.
		return;
	}

	if (!dllModule) return;

	spdlog::info("Applying D3D fixes on module 0x{:X}", reinterpret_cast<uintptr_t>(dllModule));

	std::vector<std::uint8_t*> ResolutionInstructionsScans2Result = Memory::PatternScan(dllModule, "89 2D ?? ?? ?? ?? B8 01 00 00 00 5E 89 1D ?? ?? ?? ??", "89 15 ?? ?? ?? ?? 8B 86 0C 06 00 00 A3 ?? ?? ?? ??", "89 0D ?? ?? ?? ?? 89 15");
	if (Memory::AreAllSignaturesValid(ResolutionInstructionsScans2Result) == true)
	{
		spdlog::info("Resolution Instructions 3 Scan: Address is D3DDRV.DLL+{:x}", ResolutionInstructionsScans2Result[Resolution3Scan] - (std::uint8_t*)dllModule);
		
		spdlog::info("Resolution Instructions 4 Scan: Address is D3DDRV.DLL+{:x}", ResolutionInstructionsScans2Result[Resolution4Scan] - (std::uint8_t*)dllModule);

		spdlog::info("Resolution Instructions 5 Scan: Address is D3DDRV.DLL+{:x}", ResolutionInstructionsScans2Result[Resolution5Scan] - (std::uint8_t*)dllModule);

		ResolutionWidth3Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScans2Result[Resolution3Scan] + 2, Memory::PointerMode::Absolute);

		ResolutionHeight3Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScans2Result[Resolution3Scan] + 14, Memory::PointerMode::Absolute);
		
		ResolutionWidth4Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScans2Result[Resolution4Scan] + 2, Memory::PointerMode::Absolute);
		
		ResolutionHeight4Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScans2Result[Resolution4Scan] + 13, Memory::PointerMode::Absolute);

		ResolutionWidth5Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScans2Result[Resolution5Scan] + 2, Memory::PointerMode::Absolute);

		ResolutionHeight5Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructionsScans2Result[Resolution5Scan] + 8, Memory::PointerMode::Absolute);

		Memory::PatchBytes(ResolutionInstructionsScans2Result[Resolution3Scan], 6);

		static SafetyHookMid g_ResolutionWidthInstruction3MidHook{};

		g_ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScans2Result[Resolution3Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth3Address) = iCurrentResX;
			});

		Memory::PatchBytes(ResolutionInstructionsScans2Result[Resolution3Scan] + 12, 6);

		static SafetyHookMid g_ResolutionHeightInstruction3MidHook{};

		g_ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScans2Result[Resolution3Scan] + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight3Address) = iCurrentResY;
			});

		Memory::PatchBytes(ResolutionInstructionsScans2Result[Resolution4Scan], 6);

		static SafetyHookMid g_ResolutionWidthInstruction4MidHook{};

		g_ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionInstructionsScans2Result[Resolution4Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth4Address) = iCurrentResX;
			});

		Memory::PatchBytes(ResolutionInstructionsScans2Result[Resolution4Scan] + 12, "\x90\x90\x90\x90\x90", 5);

		static SafetyHookMid g_ResolutionHeightInstruction4MidHook{};

		g_ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionInstructionsScans2Result[Resolution4Scan] + 12, [](SafetyHookContext& ctx)
		{
			*reinterpret_cast<int*>(ResolutionHeight4Address) = iCurrentResY;
		});

		Memory::PatchBytes(ResolutionInstructionsScans2Result[Resolution5Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

		static SafetyHookMid g_ResolutionInstructions5MidHook{};

		g_ResolutionInstructions5MidHook = safetyhook::create_mid(ResolutionInstructionsScans2Result[Resolution5Scan], [](SafetyHookContext& ctx)
		{
			*reinterpret_cast<int*>(ResolutionWidth5Address) = iCurrentResX;

			*reinterpret_cast<int*>(ResolutionHeight5Address) = iCurrentResY;
		});

		bD3DPatched.store(true);

		g_lastD3DModule.store(dllModule);

		spdlog::info("D3D patches/hooks applied successfully.");
	}
}

void CleanupD3DFix()
{
	std::lock_guard<std::mutex> lock(g_patchMutex);

	if (!bD3DPatched.load()) return;

	bD3DPatched.store(false);

	g_lastD3DModule.store(nullptr);

	spdlog::info("Detected D3DDRV.DLL unload - leaving patched bytes, marking as unapplied.");
}

DWORD __stdcall D3DWatcherThread(void*)
{
	while (true)
	{
		HMODULE cur = GetModuleHandleA("D3DDRV.DLL");

		if (cur && !bD3DPatched.load())
		{
			ApplyD3DFix(cur);
		}			
		else if (!cur && bD3DPatched.load())
		{
			CleanupD3DFix();
		}			
		else if (cur && g_lastD3DModule.load() && cur != g_lastD3DModule.load())
		{
			// new base, mark as unapplied and reapply
			CleanupD3DFix();
			ApplyD3DFix(cur);
		}

		Sleep(200);
	}

	return 0;
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