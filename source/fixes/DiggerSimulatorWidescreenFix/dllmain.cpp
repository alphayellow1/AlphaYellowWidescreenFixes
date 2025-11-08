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
std::string sFixName = "DiggerSimulatorWidescreenFix";
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
float fFOVFactor;
float fAspectRatioScale;

// Variables
float fNewCameraFOV;
float fNewAspectRatio;
uint8_t* ResolutionWidth1Address;
uint8_t* ResolutionHeight1Address;
uint8_t* ResolutionWidth2Address;
uint8_t* ResolutionHeight2Address;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	DS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DS, {"Digger Simulator", "Game.exe"}},
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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
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
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

	fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::DS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(dllModule2, "A3 ?? ?? ?? ?? 8B 8E B0 00 00 00 8B C7 5F 5D 5B 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is acknex.dll+{:x}", ResolutionInstructions1ScanResult - (std::uint8_t*)dllModule2);

			ResolutionWidth1Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions1ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight1Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions1ScanResult + 18, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions1ScanResult + 16, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult + 16, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(dllModule2, "A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 2B CA 85 C0 89 15 ?? ?? ?? ?? 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is acknex.dll+{:x}", ResolutionInstructions2ScanResult - (std::uint8_t*)dllModule2);

			ResolutionWidth2Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions2ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight2Address = Memory::GetPointerFromAddress<uint32_t>(ResolutionInstructions2ScanResult + 22, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth2Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions2ScanResult + 20, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult + 20, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight2Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(dllModule2, "89 86 AC 00 00 00 89 8E B0 00 00 00 89 46 68 89 4E 6C 74 31");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is acknex.dll+{:x}", ResolutionInstructions3ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 18);

			static SafetyHookMid ResolutionInstructions3MidHook{};

			ResolutionInstructions3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xAC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0xB0) = iCurrentResY;

				*reinterpret_cast<int*>(ctx.esi + 0x68) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.esi + 0x6C) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D8 D1 DF E0 DD D9 F6 C4 05 7B E5 DD D8 D9 05 ?? ?? ?? ?? DC 0D ?? ?? ?? ??");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is acknex.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			CameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionScanResult + 15, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult + 13, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 13, CameraFOVInstructionMidHook);
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