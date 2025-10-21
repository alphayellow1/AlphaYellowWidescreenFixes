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
std::string sFixName = "MatHoffmansProBMXFOVFix";
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
int32_t iNewCameraFOV1;
int32_t iNewCameraFOV2;
float fNewCameraFOV;

std::atomic<bool> bSOURCEPatched{ false };
std::atomic<HMODULE> g_lastSOURCEModule{ nullptr };
std::mutex g_patchMutex;

// Game detection
enum class Game
{
	MHPB,
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
	{Game::MHPB, {"Mat Hoffman's Pro BMX", "BMX.exe"}},
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

static DWORD __stdcall DLLWatcherThread(void*);

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

	HANDLE hWatcher = CreateThread(nullptr, 0, DLLWatcherThread, nullptr, 0, &watcherThreadId);
	if (hWatcher)
	{
		SetThreadPriority(hWatcher, THREAD_PRIORITY_NORMAL);
		CloseHandle(hWatcher);
	}

	return true;
}

void FOVFix(HMODULE dllModule)
{
	std::lock_guard<std::mutex> lock(g_patchMutex);

	if (bSOURCEPatched.load())
	{
		// Already applied — skip.
		return;
	}

	if (!dllModule) return;

	spdlog::info("Applying fixes on module 0x{:X}", reinterpret_cast<uintptr_t>(dllModule));

	if (eGameType == Game::MHPB && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule, "F7 3D ?? ?? ?? ?? 33 C9", "0F AF 2D ?? ?? ?? ?? 66 8B", "0F AF 15 ?? ?? ?? ?? C1 FA ?? 66 89 ?? 0F BF 50");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1 Scan: Address is SOURCE.DLL+{:x}", CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)dllModule);

			spdlog::info("Camera FOV Instruction 2 Scan: Address is SOURCE.DLL+{:x}", CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)dllModule);

			spdlog::info("Camera FOV Instruction 3 Scan: Address is SOURCE.DLL+{:x}", CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)dllModule);

			iNewCameraFOV1 = (int32_t)((4096.0f / fAspectRatioScale) / fFOVFactor);

			iNewCameraFOV2 = 4096;

			int* iNewCameraFOV1Ptr = &iNewCameraFOV1;

			int* iNewCameraFOV2Ptr = &iNewCameraFOV2;

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV1Scan] + 2, iNewCameraFOV1Ptr);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV2Scan] + 3, iNewCameraFOV2Ptr);

			Memory::Write(CameraFOVInstructionsScansResult[CameraFOV3Scan] + 3, iNewCameraFOV2Ptr);
		}
	}

	bSOURCEPatched.store(true);

	g_lastSOURCEModule.store(dllModule);

	spdlog::info("Patches/hooks applied successfully.");
}

void CleanupFix()
{
	std::lock_guard<std::mutex> lock(g_patchMutex);

	if (!bSOURCEPatched.load()) return;

	bSOURCEPatched.store(false);

	g_lastSOURCEModule.store(nullptr);

	spdlog::info("Detected SOURCE.DLL unload - leaving patched bytes, marking as unapplied.");
}

DWORD __stdcall DLLWatcherThread(void*)
{
	while (true)
	{
		HMODULE cur = GetModuleHandleA("SOURCE.DLL");

		if (cur && !bSOURCEPatched.load())
		{
			FOVFix(cur);
		}			
		else if (!cur && bSOURCEPatched.load())
		{
			CleanupFix();
		}			
		else if (cur && g_lastSOURCEModule.load() && cur != g_lastSOURCEModule.load())
		{
			// new base, mark as unapplied and reapply
			CleanupFix();
			FOVFix(cur);
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
		return TRUE;
	}
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
