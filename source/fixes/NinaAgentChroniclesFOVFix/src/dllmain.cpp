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
#include <cmath> // For atan, tan
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
std::string sFixName = "NinaAgentChroniclesFOVFix";
std::string sFixVersion = "1.5";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool bFixFOV = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

const float epsilon = 0.00001f;

// Game detection
enum class Game
{
	NAA,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::NAA, {"Nina: Agent Chronicles", "lithtech.exe"}},
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
	inipp::get_value(ini.sections["FOV"], "Enabled", bFixFOV);
	spdlog_confparse(bFixFOV);

	// Load resolution from ini
	inipp::get_value(ini.sections["Resolution"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Resolution"], "Height", iCurrentResY);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);

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
			break;
		}
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
		}
	}

	Sleep(2000);

	dllModule = GetModuleHandleA("cshell.dll");
	if (!dllModule)
	{
		spdlog::error("Failed to get handle for cshell.dll.");
		return false;
	}

	spdlog::info("Successfully obtained handle for cshell.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule));

	return true;
}

void FOV()
{
	std::uint8_t* NAA_HipfireAndCutscenesHFOVScanResult = Memory::PatternScan(dllModule, "89 B0 C4 00 00 00 8B 04 91");
	if (NAA_HipfireAndCutscenesHFOVScanResult) {
		spdlog::info("Hipfire and Cutscenes HFOV: Address is cshell.dll+{:x}", NAA_HipfireAndCutscenesHFOVScanResult - (std::uint8_t*)dllModule);
		static SafetyHookMid NAA_HipfireAndCutscenesHFOVMidHook{};
		NAA_HipfireAndCutscenesHFOVMidHook = safetyhook::create_mid(NAA_HipfireAndCutscenesHFOVScanResult,
			[](SafetyHookContext& ctx) {
			if (ctx.esi == std::bit_cast<uint32_t>(1.5707963705062866f))
			{
				ctx.esi = std::bit_cast<uint32_t>(2.0f * atanf(tanf(1.5707963705062866f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio)));
			}
			else if (ctx.esi == std::bit_cast<uint32_t>(1.483529806137085f))
			{
				ctx.esi = std::bit_cast<uint32_t>(2.0f * atanf(tanf(1.483529806137085f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio)));
			}
			else if (ctx.esi == std::bit_cast<uint32_t>(1.7453292608261108f))
			{
				ctx.esi = std::bit_cast<uint32_t>(2.0f * atanf(tanf(1.7453292608261108f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio)));
			}
			else if (ctx.esi == std::bit_cast<uint32_t>(1.4169141054153442f))
			{
				ctx.esi = std::bit_cast<uint32_t>(2.0f * atanf(tanf(1.4169141054153442f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio)));
			}
			else if (ctx.esi == std::bit_cast<uint32_t>(1.3089969158172607f))
			{
				ctx.esi = std::bit_cast<uint32_t>(2.0f * atanf(tanf(1.3089969158172607f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio)));
			}
		});
	}

	if (eGameType == Game::NAA) {
		std::uint8_t* NAA_HipfireAndCutscenesHFOVScanResult2 = Memory::PatternScan(exeModule, "89 81 C0 01 00 00 C3 90 90 90");
		if (NAA_HipfireAndCutscenesHFOVScanResult2) {
			spdlog::info("Hipfire and Cutscenes HFOV: Address is {:s}+{:x}", sExeName.c_str(), NAA_HipfireAndCutscenesHFOVScanResult2 - (std::uint8_t*)exeModule);
			static SafetyHookMid NAA_HipfireAndCutscenesHFOV2MidHook{};
			NAA_HipfireAndCutscenesHFOV2MidHook = safetyhook::create_mid(NAA_HipfireAndCutscenesHFOVScanResult2,
				[](SafetyHookContext& ctx) {
				if (ctx.eax == std::bit_cast<uint32_t>(1.3089969158172607f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(1.3089969158172607f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio)));
				}
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
		FOV();
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