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
HMODULE dllModule2 = nullptr;
HMODULE pluginModule = nullptr;

// Fix details
std::string sFixName = "DraculaDaysOfGoreWidescreenFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	DATFOD,
	Unknown
};

enum ResolutionListsIndices
{
	ResList1Scan,
	ResList2Scan,
	ResList3Scan,
	ResList4Scan,
	ResList5Scan,
	ResList6Scan,
	ResList7Scan,
	ResList8Scan,
	ResList9Scan,
	ResList10Scan,
	ResList11Scan,
	ResList12Scan,
	ResList13Scan,
	ResList14Scan,
	ResList15Scan,
	ResList16Scan,
	ResList17Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DATFOD, {"Dracula: Days of Gore", "Dracula.exe"}},
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

	pluginModule = Memory::GetHandle("DraculaGamePlugin.vplugin");

	dllModule2 = Memory::GetHandle("vision71.dll");

	return true;
}

static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::DATFOD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionListsScansResult = Memory::PatternScan(exeModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 00 00 48 42 54 69 6D", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 47 61 6D 65 44 61 74 61", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 10 32 40 00 68 A4",
		pluginModule, "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 61 0B 36 3B 4B 69 6C 6C 4D 6F 6E 73 74 65 72 41 63 74", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 53 61 6D 70 6C 65 20 67 61 6D 65 20 70 6C 75 67 69 6E 3A 49 6E 69 74 45 6E 67 69 6E 65 50 6C", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 53 6F 75 6E 64 73 00 00 2E 2E 2E 66 61 69 6C 65 64 21 00 00 4C",
		"20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 4D 6F 6E 73 74 65 72 45 6E 74 69 74 79 00 00 00 70 61 72 74 69 63", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 00 00 7A 43", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 70 61 72 74", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 40 67", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 48 6F",
		"20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 61 0B 36 3B 41 74 74", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 31 00 00 00", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 44 45 41 54 48", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 61 0B 36 3B 70 61 72 74 69 63",
		"20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 DB 0F 49 40 41 49 4E 6F 64 65", "20 03 00 00 00 04 00 00 00 05 00 00 58 02 00 00 00 03 00 00 00 04 00 00 50 6C 61 79");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResList1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResList2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution List 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListsScansResult[ResList3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution List 4 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList4Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 5 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList5Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 6 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList6Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 7 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList7Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 8 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList8Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 9 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList9Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 10 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList10Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 11 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList11Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 12 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList12Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 13 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList13Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 14 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList14Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 15 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList15Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 16 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList16Scan] - (std::uint8_t*)pluginModule);

			spdlog::info("Resolution List 17 Scan: Address is DraculaGamePlugin.vplugin+{:x}", ResolutionListsScansResult[ResList17Scan] - (std::uint8_t*)pluginModule);

			Memory::Write(ResolutionListsScansResult[ResList1Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList1Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList2Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList2Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList3Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList3Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList3Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList3Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList3Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList3Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList4Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList4Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList4Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList4Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList4Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList4Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList5Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList5Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList5Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList5Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList5Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList5Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList6Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList6Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList6Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList6Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList6Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList6Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList7Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList7Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList7Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList7Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList7Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList7Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList8Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList8Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList8Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList8Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList8Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList8Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList9Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList9Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList9Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList9Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList9Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList9Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList10Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList10Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList10Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList10Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList10Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList10Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList11Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList11Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList11Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList11Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList11Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList11Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList12Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList12Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList12Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList12Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList12Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList12Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList13Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList13Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList13Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList13Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList13Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList13Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList14Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList14Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList14Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList14Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList14Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList14Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList15Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList15Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList15Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList15Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList15Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList15Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList16Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList16Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList16Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList16Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList16Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList16Scan] + 20, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList17Scan], iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList17Scan] + 4, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList17Scan] + 8, iCurrentResX);

			Memory::Write(ResolutionListsScansResult[ResList17Scan] + 12, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList17Scan] + 16, iCurrentResY);

			Memory::Write(ResolutionListsScansResult[ResList17Scan] + 20, iCurrentResY);
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 86 C4 00 00 00 52 50 B9 ?? ?? ?? ?? E8 ?? ?? ?? ??");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is vision71.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0xC4);

				if (fCurrentCameraFOV == 90.0f)
				{
					fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale);
				}

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
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