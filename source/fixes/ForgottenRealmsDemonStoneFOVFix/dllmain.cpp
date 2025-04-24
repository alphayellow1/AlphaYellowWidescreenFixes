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
std::string sFixName = "ForgottenRealmsDemonStoneFOVFix";
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
float fNewAspectRatio;

// Game detection
enum class Game
{
	FRDS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FRDS, {"Forgotten Realms: Demon Stone", "Demonstone.exe"}},
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
	if (eGameType == Game::FRDS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* AspectRatio1ScanResult = Memory::PatternScan(exeModule, "C7 46 10 AB AA AA 3F E8 FA C7 04 00 57 8D 8E 44");
		if (AspectRatio1ScanResult)
		{
			spdlog::info("Aspect Ratio 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio1ScanResult + 3 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio1ScanResult + 3, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatio2ScanResult = Memory::PatternScan(exeModule, "68 AB AA AA 3F 52 8D 8C 24 80 11 00 00 E8 6D");
		if (AspectRatio2ScanResult)
		{
			spdlog::info("Aspect Ratio 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio2ScanResult + 1 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio2ScanResult + 1, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatio3ScanResult = Memory::PatternScan(exeModule, "68 AB AA AA 3F 68 D8 0F 49 3F 8B CF C7 46 4C 01 00 00");
		if (AspectRatio3ScanResult)
		{
			spdlog::info("Aspect Ratio 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio3ScanResult + 1 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio3ScanResult + 1, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 3 memory address.");
			return;
		}

		std::uint8_t* AspectRatio4ScanResult = Memory::PatternScan(exeModule, "68 AB AA AA 3F 51 8D 8E 08 01 00 00 D9 1C 24 E8 55 73 F3");
		if (AspectRatio4ScanResult)
		{
			spdlog::info("Aspect Ratio 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio4ScanResult + 1 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio4ScanResult + 1, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 4 memory address.");
			return;
		}

		std::uint8_t* AspectRatio5ScanResult = Memory::PatternScan(exeModule, "68 AB AA AA 3F 51 8B CD D9 1C 24 E8 32 72");
		if (AspectRatio5ScanResult)
		{
			spdlog::info("Aspect Ratio 5: Address is {:s}+{:x}", sExeName.c_str(), AspectRatio5ScanResult + 1 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatio5ScanResult + 1, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio 5 memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "0F BF 91 84 06 00 00 89 54 24 04 8B 81 A8 08 00 00 50 DB 44 24 08");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				uint16_t iNewResX = (uint16_t)(480.0f * fNewAspectRatio);

				*reinterpret_cast<uint16_t*>(ctx.ecx + 0x684) = iNewResX;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera HFOV instruction memory address.");
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