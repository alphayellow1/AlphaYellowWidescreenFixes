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
HMODULE dllModule2;
HMODULE thisModule;

// Fix details
std::string sFixName = "ChameleonFOVFix";
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
bool FixActive;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;
float fFOVFactor;

// Game detection
enum class Game
{
	CHAMELEON,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CHAMELEON, {"Chameleon", "Chameleon.exe"}},
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
	}

	dllModule2 = GetModuleHandleA("LS3DF.dll");
	if (!dllModule2)
	{
		spdlog::error("Failed to get handle for LS3DF.dll.");
		return false;
	}

	spdlog::info("Successfully obtained handle for LS3DF.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

void FOVFix()
{
	if (eGameType == Game::CHAMELEON) {
		std::uint8_t* Chameleon_AspectRatioScanResult = Memory::PatternScan(dllModule2, "D8 0D ?? ?? ?? ?? D9 82 20 01 00 00 D8 1D ?? ?? ?? ??");
		if (Chameleon_AspectRatioScanResult) {
			spdlog::info("Aspect Ratio: Address is LS3DF.dll+{:x}", Chameleon_AspectRatioScanResult - (std::uint8_t*)dllModule2);
			static SafetyHookMid Chameleon_AspectRatioMidHook{};
			Chameleon_AspectRatioMidHook = safetyhook::create_mid(Chameleon_AspectRatioScanResult + 0x6,
				[](SafetyHookContext& ctx) {
				*reinterpret_cast<float*>(ctx.edx + 0x120) = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);
			});
		}

		std::uint8_t* Chameleon_CameraFOVScanResult = Memory::PatternScan(dllModule2, "8B 82 EC 00 00 00 5F 40 5E 89 82 EC 00 00 00 5B C3 D9 82 08 01 00 00");
		if (Chameleon_CameraFOVScanResult) {
			spdlog::info("Camera FOV: Address is LS3DF.dll+{:x}", Chameleon_CameraFOVScanResult - (std::uint8_t*)dllModule2);
			static SafetyHookMid Chameleon_CameraFOVMidHook{};
			Chameleon_CameraFOVMidHook = safetyhook::create_mid(Chameleon_CameraFOVScanResult + 0x11,
				[](SafetyHookContext& ctx) {
				if (*reinterpret_cast<float*>(ctx.edx + 0x108) == 1.308996916f || *reinterpret_cast<float*>(ctx.edx + 0x108) == 1.134464025f || *reinterpret_cast<float*>(ctx.edx + 0x108) == 2.094395161f) {
					*reinterpret_cast<float*>(ctx.edx + 0x108) = fFOVFactor * (2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.edx + 0x108) / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio)));
				}
			});
		}

		std::uint8_t* Chameleon_AspectRatioComparisonScanResult = Memory::PatternScan(dllModule2, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 41 75 19 D9 C0 DE C1");

		std::uint8_t* Chameleon_CodecaveScanResult = Memory::PatternScan(dllModule2, "E9 5D 36 FF FF 00 00 00 00 00 00");

		std::uint8_t* Chameleon_CodecaveValueAddress = Memory::GetAbsolute(Chameleon_CodecaveScanResult + 0x7);

		Memory::Write(Chameleon_AspectRatioComparisonScanResult + 0x2, Chameleon_CodecaveValueAddress - 0x4);

		Memory::Write(Chameleon_CodecaveScanResult + 0x7, 8.0f);
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