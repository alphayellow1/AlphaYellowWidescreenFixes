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
HMODULE dllModule2;
HMODULE dllModule3;
HMODULE thisModule;

// Fix details
std::string sFixName = "AdventurePinballForgottenIslandResolutionFix";
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixActive = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
float fNewCameraHFOV;
float fNewCameraFOV;
float fFOVFactor;

// Game detection
enum class Game
{
	APFI,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::APFI, {"Adventure Pinball: Forgotten Island", "PB.exe"}},
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
	inipp::get_value(ini.sections["ResolutionFix"], "Enabled", FixActive);
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
		}
	}

	Sleep(1000);

	dllModule2 = GetModuleHandleA("D3DDrv.dll");
	if (!dllModule2)
	{
		spdlog::error("Failed to get handle for D3DDrv.dll.");
		return false;
	}

	spdlog::info("Successfully obtained handle for D3DDrv.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	dllModule3 = GetModuleHandleA("WinDrv.dll");
	if (!dllModule2)
	{
		spdlog::error("Failed to get handle for WinDrv.dll.");
		return false;
	}

	spdlog::info("Successfully obtained handle for WinDrv.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

void ResolutionFix()
{
	if (eGameType == Game::APFI && FixActive == true)
	{
		std::uint8_t* APFI_ResolutionsScanResult = Memory::PatternScan(dllModule2, "3D ?? ?? ?? ?? 72 4B 75 09 81 7E 08 ?? ?? ?? ?? 74 40 3D ?? ?? ?? ?? 75 09 81 7E 08 ?? ?? ?? ?? 74 30 3D ?? ?? ?? ?? 75 09 81 7E 08 ?? ?? ?? ?? 74 20 3D ?? ?? ?? ?? 75 09 81 7E 08 ?? ?? ?? ?? 74 10 3D ?? ?? ?? ?? 75 5A 81 7E 08 ?? ?? ?? ?? 75 51");
		spdlog::info("Resolutions: Address is D3DDrv.dll+{:x}", APFI_ResolutionsScanResult - (std::uint8_t*)dllModule2);

		Memory::Write(APFI_ResolutionsScanResult + 0x1, iCurrentResX);

		Memory::Write(APFI_ResolutionsScanResult + 0xC, iCurrentResY);

		Memory::Write(APFI_ResolutionsScanResult + 0x13, iCurrentResX);

		Memory::Write(APFI_ResolutionsScanResult + 0x1C, iCurrentResY);

		Memory::Write(APFI_ResolutionsScanResult + 0x23, iCurrentResX);

		Memory::Write(APFI_ResolutionsScanResult + 0x2C, iCurrentResY);

		Memory::Write(APFI_ResolutionsScanResult + 0x33, iCurrentResX);

		Memory::Write(APFI_ResolutionsScanResult + 0x3C, iCurrentResY);

		Memory::Write(APFI_ResolutionsScanResult + 0x43, iCurrentResX);

		Memory::Write(APFI_ResolutionsScanResult + 0x4C, iCurrentResY);

		std::uint8_t* APFI_ResolutionScan1Result = Memory::PatternScan(dllModule3, "89 51 64 8B 55 E8");
		if (APFI_ResolutionScan1Result) {
			spdlog::info("Resolution 1: Address is WinDrv.dll+{:x}", APFI_ResolutionScan1Result - (std::uint8_t*)dllModule3);
			static SafetyHookMid APFI_Resolution1MidHook{};
			APFI_Resolution1MidHook = safetyhook::create_mid(APFI_ResolutionScan1Result,
				[](SafetyHookContext& ctx) {
				ctx.edx = std::bit_cast<uint32_t>(iCurrentResX);
			});
		}

		std::uint8_t* APFI_ResolutionScan2Result = Memory::PatternScan(dllModule3, "89 41 68 8D 04 D5 00 00 00 00");
		if (APFI_ResolutionScan2Result) {
			spdlog::info("Resolution 2: Address is WinDrv.dll+{:x}", APFI_ResolutionScan2Result - (std::uint8_t*)dllModule3);
			static SafetyHookMid APFI_Resolution2MidHook{};
			APFI_Resolution2MidHook = safetyhook::create_mid(APFI_ResolutionScan2Result,
				[](SafetyHookContext& ctx) {
				ctx.eax = std::bit_cast<uint32_t>(iCurrentResY);
			});
		}

		std::uint8_t* APFI_ResolutionScan3Result = Memory::PatternScan(dllModule3, "8B 78 64 EB 03 8B 78 58 89 7D 0C 39 55 10 75 0F");
		if (APFI_ResolutionScan3Result) {
			spdlog::info("Resolution 2: Address is WinDrv.dll+{:x}", APFI_ResolutionScan3Result - (std::uint8_t*)dllModule3);
			static SafetyHookMid APFI_Resolution3MidHook{};
			APFI_Resolution3MidHook = safetyhook::create_mid(APFI_ResolutionScan3Result,
				[](SafetyHookContext& ctx) {
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x64) = iCurrentResX;
			});
		}

		std::uint8_t* APFI_ResolutionScan4Result = Memory::PatternScan(dllModule3, "8B 78 68 EB 03 8B 78 5C 89 7D 10 8B 7D 14 3B FA");
		if (APFI_ResolutionScan4Result) {
			spdlog::info("Resolution 2: Address is WinDrv.dll+{:x}", APFI_ResolutionScan4Result - (std::uint8_t*)dllModule3);
			static SafetyHookMid APFI_Resolution4MidHook{};
			APFI_Resolution4MidHook = safetyhook::create_mid(APFI_ResolutionScan4Result,
				[](SafetyHookContext& ctx) {
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x68) = iCurrentResY;
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
		ResolutionFix();
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