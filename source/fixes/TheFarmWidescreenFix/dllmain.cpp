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
std::string sFixName = "TheFarmWidescreenFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;

// Variables
float fNewCameraFOV;
float fFOVFactor;
float fNewAspectRatio;
float fAspectRatioScale;
uint8_t* CameraFOVValueAddress;

// Game detection
enum class Game
{
	TF,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TF, {"The Farm", "Start.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("acknex.dll")) == nullptr)
	{
		spdlog::warn("acknex.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for acknex.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid AspectRatioInstruction1Hook{};

void AspectRatioInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fdiv dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid AspectRatioInstruction2Hook{};

void AspectRatioInstruction2MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewAspectRatio]
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVValueAddress);

	fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::TF && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(dllModule2, "20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 00 05 00 00 00 04 00 00 78 05 00 00 1A 04 00 00 40 06 00 00 B0 04 00 00 80 07 00 00 B0 04 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is acknex.dll+{:x}", ResolutionListScanResult - (std::uint8_t*)dllModule2);
			
			// 800x600
			Memory::Write(ResolutionListScanResult, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 4, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 8, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 12, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 16, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 20, iCurrentResY);

			// 1440x1050
			Memory::Write(ResolutionListScanResult + 24, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 28, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionListScanResult + 32, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 36, iCurrentResY);

			// 1920x1200
			Memory::Write(ResolutionListScanResult + 40, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 44, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction1ScanResult = Memory::PatternScan(dllModule2, "D8 35 14 48 6E 10 D9 5C 24 04 D9 44 24 04");
		if (AspectRatioInstruction1ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is acknex.dll+{:x}", AspectRatioInstruction1ScanResult - (std::uint8_t*)dllModule2);
			
			Memory::PatchBytes(AspectRatioInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstruction1ScanResult, AspectRatioInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? DD 1C 24 E8 ?? ?? ?? ?? 6A 01 D9 1D ?? ?? ?? ??");
		if (AspectRatioInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 2: Address is acknex.dll+{:x}", AspectRatioInstruction2ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(AspectRatioInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstruction2ScanResult, AspectRatioInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? DC C0 D9 1D ?? ?? ?? ?? DB 04 24");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is acknex.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			CameraFOVValueAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

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