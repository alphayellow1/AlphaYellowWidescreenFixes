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

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "WildRidesWaterParkFactoryWidescreenFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraFOV;
static uint8_t* CameraFOVValueAddress;
static uint8_t* AspectRatioValueAddress;

// Function to convert degrees to radians
float DegToRad(float degrees)
{
	return degrees * (fPi / 180.0f);
}

// Function to convert radians to degrees
float RadToDeg(float radians)
{
	return radians * (180.0f / fPi);
}

// Game detection
enum class Game
{
	WRWPF,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::WRWPF, {"Wild Rides: WaterPark Factory", "WildRides.exe"}},
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

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * (fNewAspectRatio / fOldAspectRatio)));
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVValueAddress);

	fNewCameraFOV = fFOVFactor * CalculateNewFOV(fCurrentCameraFOV);

	_asm
	{
		fmul dword ptr ds:[fNewCameraFOV]
	}
}

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentAspectRatio = *reinterpret_cast<float*>(AspectRatioValueAddress);

	fCurrentAspectRatio = fNewAspectRatio;

	_asm
	{
		fld dword ptr ds:[fNewAspectRatio]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::WRWPF && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		
		std::uint8_t* ResolutionList1ScanResult = Memory::PatternScan(exeModule, "C7 01 00 04 00 00 C7 02 00 03 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 80 04 00 00 C7 01 60 03 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 05 00 00 C7 00 C0 03 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 40 06 00 00 C7 02 B0 04 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 80 02 00 00 C7 01 E0 01 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 05 00 00 C7 00 00 03 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 00 05 00 00 C7 02 00 04 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 20 03 00 00 C7 01 58 02 00 00 C3 8D 49 00 65 F5 4B 00 7A F5 4B 00 8F F5 4B 00 A4 F5 4B 00 B9 F5 4B 00 CE F5 4B 00 E3 F5 4B 00 90 90 90 90 8B 44 24 04 8B 4C 24 08 C7 00 20 03 00 00 C7 01 58 02 00 00");
		if (ResolutionList1ScanResult)
		{
			spdlog::info("Resolution List 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionList1ScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionList1ScanResult + 2, iCurrentResX); // 1024

			Memory::Write(ResolutionList1ScanResult + 8, iCurrentResY); // 768

			Memory::Write(ResolutionList1ScanResult + 23, iCurrentResX); // 1152

			Memory::Write(ResolutionList1ScanResult + 29, iCurrentResY); // 864

			Memory::Write(ResolutionList1ScanResult + 44, iCurrentResX); // 1280

			Memory::Write(ResolutionList1ScanResult + 50, iCurrentResY); // 960

			/*
			Memory::Write(ResolutionList1ScanResult + 65, iCurrentResX); // 1600

			Memory::Write(ResolutionList1ScanResult + 71, iCurrentResY); // 1200
			*/

			Memory::Write(ResolutionList1ScanResult + 86, iCurrentResX); // 640

			Memory::Write(ResolutionList1ScanResult + 92, iCurrentResY); // 480
			
			Memory::Write(ResolutionList1ScanResult + 107, iCurrentResX); // 1280

			Memory::Write(ResolutionList1ScanResult + 113, iCurrentResY); // 768

			Memory::Write(ResolutionList1ScanResult + 128, iCurrentResX); // 1280

			Memory::Write(ResolutionList1ScanResult + 134, iCurrentResY); // 1024

			Memory::Write(ResolutionList1ScanResult + 149, iCurrentResX); // 800

			Memory::Write(ResolutionList1ScanResult + 155, iCurrentResY); // 600

			/* HUD Resolution - Causes most of the HUD to be stuck in the left side of the screen
			Memory::Write(ResolutionList1ScanResult + 205, (int)(600 * fNewAspectRatio)); // 800

			Memory::Write(ResolutionList1ScanResult + 211, 600); // 600
		    */
		}
		else
		{
			spdlog::error("Failed to locate resolutions instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D8 C9 D9 1D ?? ?? ?? ?? DD D8 D9 05 ?? ?? ?? ?? D8 4C 24 00");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

			uint32_t imm1 = *reinterpret_cast<uint32_t*>(AspectRatioInstructionScanResult + 2);

			AspectRatioValueAddress = reinterpret_cast<uint8_t*>(imm1);

			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? C7 05 ?? ?? ?? ?? 00 00 80 3F C7 05 ?? ?? ?? ?? 00 00 00 00");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			uint32_t imm2 = *reinterpret_cast<uint32_t*>(CameraFOVInstructionScanResult + 2);

			CameraFOVValueAddress = reinterpret_cast<uint8_t*>(imm2);
			
			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

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