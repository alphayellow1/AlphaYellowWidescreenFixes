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
HMODULE dllModule2;
HMODULE thisModule;

// Fix details
std::string sFixName = "Hidden&DangerousDeluxeFOVFix";
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

// FOV 
std::pair DesktopDimensions = { 0,0 };
const float fNativeAspect = 4.0f / 3.0f;
float fNewCameraHFOV;
float fNewCameraFOV;

// Ini variables
bool FixActive;

// New INI variables
float fNewRenderingDistance;
float fFOVFactor;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

// Game detection
enum class Game
{
	HDD,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::HDD, {"Hidden & Dangerous Deluxe", "hde.exe"}},
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
		// Use std::filesystem::path operations for clarity
		std::filesystem::path logFilePath = sExePath / sLogFile;
		logger = spdlog::basic_logger_st(sFixName, logFilePath.string(), true);
		spdlog::set_default_logger(logger);
		spdlog::flush_on(spdlog::level::debug);
		spdlog::set_level(spdlog::level::debug); // Enable debug level logging

		spdlog::info("----------");
		spdlog::info("{:s} v{:s} loaded.", sFixName, sFixVersion);
		spdlog::info("----------");
		spdlog::info("Log file: {}", logFilePath.string());
		spdlog::info("----------");
		spdlog::info("Module Name: {:s}", sExeName);
		spdlog::info("Module Path: {:s}", sExePath.string());
		spdlog::info("Module Address: 0x{:X}", reinterpret_cast<uintptr_t>(exeModule));
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
	std::filesystem::path configPath = sFixPath / sConfigFile;
	std::ifstream iniFile(configPath);
	if (!iniFile)
	{
		AllocConsole();
		FILE* dummy;
		freopen_s(&dummy, "CONOUT$", "w", stdout);
		std::cout << sFixName << " v" << sFixVersion << " loaded." << std::endl;
		std::cout << "ERROR: Could not locate config file." << std::endl;
		std::cout << "ERROR: Make sure " << sConfigFile << " is located in " << sFixPath.string() << std::endl;
		spdlog::shutdown();
		FreeLibraryAndExitThread(thisModule, 1);
	}
	else
	{
		spdlog::info("Config file: {}", configPath.string());
		ini.parse(iniFile);
	}

	// Parse config
	ini.strip_trailing_comments();
	spdlog::info("----------");

	// Load settings from ini
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixActive);
	spdlog_confparse(FixActive);

	// Load new INI entries
	inipp::get_value(ini.sections["Resolution"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Resolution"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "RenderingDistance", fNewRenderingDistance);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fNewRenderingDistance);
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

	dllModule2 = GetModuleHandleA("i3d2.dll");
	if (!dllModule2)
	{
		spdlog::error("Failed to get handle for i3d2.dll.");
		return false;
	}

	spdlog::info("Successfully obtained handle for i3d2.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

void FOV()
{
	if (eGameType == Game::HDD && FixActive == true)
	{
		std::uint8_t* HDD_CodecaveScanResult = Memory::PatternScan(dllModule2, "E9 BF 22 FF FF 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00");

		std::uint8_t* HDD_CodecaveHFOVValueAddress = Memory::GetAbsolute(HDD_CodecaveScanResult + 0x7);

		std::uint8_t* HDD_CodecaveOverallFOVValueAddress = Memory::GetAbsolute(HDD_CodecaveScanResult + 0xB);

		std::uint8_t* HDD_CodecaveRenderingSidesValueAddress = Memory::GetAbsolute(HDD_CodecaveScanResult + 0xF);

		std::uint8_t* HDD_CodecaveFMULInstructionAddressInCodecave = Memory::GetAbsolute(HDD_CodecaveScanResult + 0x1A);

		std::uint8_t* HDD_OverallFOVScanResult = Memory::PatternScan(dllModule2, "D8 0D ?? ?? ?? ?? D9 C0 D9 FF D9 5D FC");

		std::uint8_t* HDD_RenderingSidesScanResult = Memory::PatternScan(dllModule2, "D8 0D ?? ?? ?? ?? 89 8D 7C FF FF FF");

		Memory::Write(HDD_OverallFOVScanResult + 0x2, HDD_CodecaveOverallFOVValueAddress - 0x4);

		Memory::Write(HDD_RenderingSidesScanResult + 0x2, HDD_CodecaveRenderingSidesValueAddress - 0x4);

		std::uint8_t* HDD_RenderingDistanceScanResult = Memory::PatternScan(dllModule2, "D9 FE D9 83 EC 01 00 00 D8 A3 E8 01 00 00"); // fld dword ptr ds:[ebx+1EC]
		if (HDD_RenderingDistanceScanResult) {
			static SafetyHookMid HDD_RenderingDistanceMidHook{};
			HDD_RenderingDistanceMidHook = safetyhook::create_mid(HDD_RenderingDistanceScanResult + 0x2,
				[](SafetyHookContext& ctx) {
				*reinterpret_cast<float*>(ctx.ebx + 0x1EC) = fNewRenderingDistance;
			});
		}

		std::uint8_t* HDD_HFOVScanResult = Memory::PatternScan(dllModule2, "D9 45 FC D8 F1 8B 4D F8 D9 1E");

		Memory::PatchBytes(HDD_HFOVScanResult, "\xE9\x9C\x4F\x0E\x00", 5);

		Memory::PatchBytes(HDD_CodecaveScanResult + 0x15, "\xD9\x45\xFC\xD8\x0D\x37\x63\x0F\x10\xD8\xF1\xE9\x54\xB0\xF1\xFF", 16);

		Memory::Write(HDD_CodecaveFMULInstructionAddressInCodecave - 0x4, HDD_CodecaveHFOVValueAddress - 0x4);

		fNewCameraHFOV = (4.0f / 3.0f) / (static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY));

		fNewCameraFOV = 0.5f * fFOVFactor;

		Memory::Write(HDD_CodecaveHFOVValueAddress - 0x4, fNewCameraHFOV);

		Memory::Write(HDD_CodecaveOverallFOVValueAddress - 0x4, fNewCameraFOV);

		Memory::Write(HDD_CodecaveRenderingSidesValueAddress - 0x4, 200.0f);
	}
}

DWORD __stdcall Main(void*)
{
	Sleep(3000);

	Logging();
	Configuration();
	if (DetectGame()) {
		FOV();
	}
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH: {
		thisModule = hModule;
		HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
		if (mainHandle) {
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