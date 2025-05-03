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
HMODULE thisModule;

// Fix details
std::string sFixName = "SegaRally2WidescreenFix";
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

// Variables
float fNewCameraFOV;
float fFOVFactor;
float fNewAspectRatio;

// Game detection
enum class Game
{
	SR2,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SR2, {"Sega Rally 2", "SEGA RALLY 2.exe"}},
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

	return true;
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fNewCameraFOV = (fNewAspectRatio / fOldAspectRatio) * 0.75f;

	_asm
	{
		push eax
		mov eax,fNewCameraFOV
		mov dword ptr ds:[0x004946D0], eax
		pop eax
		cmp dword ptr ds:[0x100123FC],0x280
		je Code2

		fld dword ptr ds:[esi + 0x4]
		fdiv dword ptr ds:[0x004946D0]
		fstp dword ptr ds:[esi + 0x4]

		Code2:
		mov edx, [esi + 0x4]
		mov dword ptr ss:[esp + 0xC], edi
	}
}

void WidescreenFix()
{
	if (eGameType == Game::SR2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* Resolution800x600ScanResult = Memory::PatternScan(exeModule, "?? 4D 00 20 03 00 00 C7 05 ?? 5E 4D 00 58 02 00 00");
		if (Resolution800x600ScanResult)
		{
			spdlog::info("Resolution 800x600 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution800x600ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(Resolution800x600ScanResult + 3, iCurrentResX);

			Memory::Write(Resolution800x600ScanResult + 13, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList1ScanResult = Memory::PatternScan(exeModule, "72 74 2E 62 67 00 00 00 00 00 00 00 00 00 00 00 80 02 00 00 E0 01 00 00 00 00 00 00 00 00 00 00 80 02 00 00 E0 00 00 00 00 00 00 00 00 01 00 00 80 02 00 00 E0 01 00 00 00 00 00 00 00 00 00 00 20 03 00 00 58 02 00 00 00 00 00 00 00 00 00 00 20 03 00 00 1C 01 00 00 00 00 00 00 3C 01 00 00 20 03 00 00 58 02 00 00");
		if (ResolutionList1ScanResult)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionList1ScanResult + 64, iCurrentResX);
			
			Memory::Write(ResolutionList1ScanResult + 68, iCurrentResY);

			Memory::Write(ResolutionList1ScanResult + 96, iCurrentResX);

			Memory::Write(ResolutionList1ScanResult + 100, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionList2ScanResult = Memory::PatternScan(exeModule, "20 03 00 00 58 02 00 00 00 00 00 3F 00 00 00 3F 00 00 80 3F 00 00 80 3F 00 00 00 00 00 00 00 00 20 03 00 00 2C 01 00 00 00 00 00 3F 00 00 00 3F 00 00 80 3F 00 00 80 3F 00 00 00 00 F0 00 00 00 20 03 00 00 58 02 00 00");
		if (ResolutionList2ScanResult)
		{
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList2ScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(ResolutionList2ScanResult, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 4, iCurrentResY);

			Memory::Write(ResolutionList2ScanResult + 64, iCurrentResX);

			Memory::Write(ResolutionList2ScanResult + 68, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list 2 scan memory address.");
			return;
		}

		std::uint8_t* HUDPositionsScanResult = Memory::PatternScan(exeModule, "00 00 13 44 00 00 00 41 00 00 46 44 00 00 C8 43 00 00 96 43");
		if (HUDPositionsScanResult)
		{
			spdlog::info("HUD Positions Scan: Address is {:s}+{:x}", sExeName.c_str(), HUDPositionsScanResult - (std::uint8_t*)exeModule);
			
			Memory::Write(HUDPositionsScanResult, iCurrentResY - 12);

			Memory::Write(HUDPositionsScanResult + 8, iCurrentResX - 8);
			
			Memory::Write(HUDPositionsScanResult + 12, iCurrentResX / 2);
			
			Memory::Write(HUDPositionsScanResult + 16, iCurrentResY / 2);
		}
		else
		{
			spdlog::error("Failed to locate HUD positions scan memory address.");
			return;
		}

		while ((dllModule2 = GetModuleHandleA("MGameD3D.dll")) == nullptr)
		{
			spdlog::warn("MGameD3D.dll not loaded yet. Waiting...");
			Sleep(10000);
		}

		spdlog::info("Successfully obtained handle for MGameD3D.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "8B 56 04 89 7C 24 0C 89 54 24 08 DF 6C 24 08");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is MGameD3D.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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