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

// Fix details
std::string sFixName = "RequiemAvengingAngelFOVFix";
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixActive;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

// Game detection
enum class Game
{
	RAA,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RAA, {"Requiem: Avenging Angel", "D3D.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixActive);
	spdlog_confparse(FixActive);

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
			return true;
		}
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
	return false;
}

void FOVFix()
{
	if (eGameType == Game::RAA && FixActive == true) {
		std::uint8_t* RAA_CameraFOVScanResult = Memory::PatternScan(exeModule, "89 86 FC 09 00 00 8B 86 38 09 00 00 51 89 8E 00 0A 00 00");
		if (RAA_CameraFOVScanResult) {
			spdlog::info("Camera FOV: Address is {:s}+{:x}", sExeName.c_str(), RAA_CameraFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid RAA_CameraFOVMidHook{};
			RAA_CameraFOVMidHook = safetyhook::create_mid(RAA_CameraFOVScanResult,
				[](SafetyHookContext& ctx) {
				std::uint8_t* RAA_CameraHFOVScanResult = Memory::PatternScan(exeModule, "00 57 8D 94 24 B0 03 00 00 8B CF C7 84 24 B0 03 00 00 ?? ?? ?? ?? C7 84 24 B4 03 00 00 00 00 00 00 C7 84 24 B8 03 00 00 00 00 00 00 C7 84 24 BC");
				spdlog::info("Camera HFOV: Address is {:s}+{:x}", sExeName.c_str(), RAA_CameraHFOVScanResult - (std::uint8_t*)exeModule);

				std::uint8_t* RAA_CameraVFOVScanResult = Memory::PatternScan(exeModule, "C7 84 24 C4 03 00 00 ?? ?? ?? ?? C7 84 24 C8 03 00 00 00 00 00 00 C7 84 24 CC 03 00 00 00 00 00 00 C7 84 24 D0 03");
				spdlog::info("Camera VFOV: Address is {:s}+{:x}", sExeName.c_str(), RAA_CameraVFOVScanResult - (std::uint8_t*)exeModule);

				if (*reinterpret_cast<float*>(ctx.esi + 0x938) < 120.0f)
				{
					Memory::Write(RAA_CameraHFOVScanResult + 0x12, (4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)));
					Memory::Write(RAA_CameraVFOVScanResult + 0x7, 1.0f);
				}
				else if (*reinterpret_cast<float*>(ctx.esi + 0x938) == 120.0f)
				{
					Memory::Write(RAA_CameraHFOVScanResult + 0x12, (4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)));
					Memory::Write(RAA_CameraVFOVScanResult + 0x7, (4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)));
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