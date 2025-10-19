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

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;

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
	inipp::get_value(ini.sections["ResolutionFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load resolution from ini
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
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

	while ((dllModule2 = GetModuleHandleA("D3DDrv.dll")) == nullptr)
	{
		spdlog::warn("D3DDrv.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for D3DDrv.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while ((dllModule3 = GetModuleHandleA("WinDrv.dll")) == nullptr)
	{
		spdlog::warn("WinDrv.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for WinDrv.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	return true;
}

void ResolutionFix()
{
	if (eGameType == Game::APFI && bFixActive == true)
	{
		std::uint8_t* ResolutionsScanResult = Memory::PatternScan(dllModule2, "3D ?? ?? ?? ?? 72 4B 75 09 81 7E 08 ?? ?? ?? ?? 74 40 3D ?? ?? ?? ?? 75 09 81 7E 08 ?? ?? ?? ?? 74 30 3D ?? ?? ?? ?? 75 09 81 7E 08 ?? ?? ?? ?? 74 20 3D ?? ?? ?? ?? 75 09 81 7E 08 ?? ?? ?? ?? 74 10 3D ?? ?? ?? ?? 75 5A 81 7E 08 ?? ?? ?? ?? 75 51");
		if (ResolutionsScanResult)
		{
			spdlog::info("Resolutions: Address is D3DDrv.dll+{:x}", ResolutionsScanResult - (std::uint8_t*)dllModule2);

			Memory::Write(ResolutionsScanResult + 1, iCurrentResX);

			Memory::Write(ResolutionsScanResult + 12, iCurrentResY);

			Memory::Write(ResolutionsScanResult + 19, iCurrentResX);

			Memory::Write(ResolutionsScanResult + 28, iCurrentResY);

			Memory::Write(ResolutionsScanResult + 35, iCurrentResX);

			Memory::Write(ResolutionsScanResult + 44, iCurrentResY);

			Memory::Write(ResolutionsScanResult + 51, iCurrentResX);

			Memory::Write(ResolutionsScanResult + 60, iCurrentResY);

			Memory::Write(ResolutionsScanResult + 67, iCurrentResX);

			Memory::Write(ResolutionsScanResult + 76, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolutions memory address.");
			return;
		}		

		std::uint8_t* ResolutionScan1Result = Memory::PatternScan(dllModule3, "89 51 64 8B 55 E8");
		if (ResolutionScan1Result)
		{
			spdlog::info("Resolution 1: Address is WinDrv.dll+{:x}", ResolutionScan1Result - (std::uint8_t*)dllModule3);

			static SafetyHookMid Resolution1MidHook{};

			Resolution1MidHook = safetyhook::create_mid(ResolutionScan1Result, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uint32_t>(iCurrentResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionScan2Result = Memory::PatternScan(dllModule3, "89 41 68 8D 04 D5 00 00 00 00");
		if (ResolutionScan2Result)
		{
			spdlog::info("Resolution 2: Address is WinDrv.dll+{:x}", ResolutionScan2Result - (std::uint8_t*)dllModule3);

			static SafetyHookMid Resolution2MidHook{};

			Resolution2MidHook = safetyhook::create_mid(ResolutionScan2Result, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uint32_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionScan3Result = Memory::PatternScan(dllModule3, "8B 78 64 EB 03 8B 78 58 89 7D 0C 39 55 10 75 0F");
		if (ResolutionScan3Result)
		{
			spdlog::info("Resolution 3: Address is WinDrv.dll+{:x}", ResolutionScan3Result - (std::uint8_t*)dllModule3);

			static SafetyHookMid Resolution3MidHook{};

			Resolution3MidHook = safetyhook::create_mid(ResolutionScan3Result, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x64) = iCurrentResX;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionScan4Result = Memory::PatternScan(dllModule3, "8B 78 68 EB 03 8B 78 5C 89 7D 10 8B 7D 14 3B FA");
		if (ResolutionScan4Result)
		{
			spdlog::info("Resolution 4: Address is WinDrv.dll+{:x}", ResolutionScan4Result - (std::uint8_t*)dllModule3);

			static SafetyHookMid Resolution4MidHook{};

			Resolution4MidHook = safetyhook::create_mid(ResolutionScan4Result, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<uint32_t*>(ctx.eax + 0x68) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution 4 scan memory address.");
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
