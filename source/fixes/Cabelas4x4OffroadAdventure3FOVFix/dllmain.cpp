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
#include <vector>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule2 = nullptr;
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "Cabelas4x4OffroadAdventure3FOVFix";
std::string sFixVersion = "1.2";
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
float fNewWeaponZoomFOV;
float fNewCameraFOV;
float fAspectRatioScale;
static uint32_t iInsideCar;
static uint8_t* InsideCarTriggerValueAddress;

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
	C4X4OA3,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::C4X4OA3, {"Cabela's 4x4 Offroad Adventure 3", "Engine.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("EngineDll.dll")) == nullptr)
	{
		spdlog::warn("EngineDll.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for EngineDll.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while ((dllModule3 = GetModuleHandleA("GameDll.dll")) == nullptr)
	{
		spdlog::warn("GameDll.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for GameDll.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	return true;
}

float CalculateNewFOV(float fCurrentFOV)
{
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * fAspectRatioScale));
}

static SafetyHookMid WeaponZoomFOVInstruction1Hook{};

void WeaponZoomFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fcomp dword ptr ds : [fNewWeaponZoomFOV]
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ebx + 0xD0);

	if (iInsideCar == 0)
	{
		if (fCurrentCameraFOV == 75.0f)
		{
			fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV) * fFOVFactor;
		}
		else
		{
			fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV);
		}
	}
	else if (iInsideCar == 1)
	{
		fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

void FOVFix()
{
	if (eGameType == Game::C4X4OA3 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		fNewWeaponZoomFOV = 30.0f;

		std::uint8_t* InsideCarTriggerInstructionScanResult = Memory::PatternScan(exeModule, "39 1D ?? ?? ?? ?? 75 17 8B 8E 64 1C 00 00 8B 86 68 1C 00 00 3B C8 C7 45 EC 01 00 00 00");
		if (InsideCarTriggerInstructionScanResult)
		{
			spdlog::info("Inside Car Trigger Instruction: Address is {:s}+{:x}", sExeName.c_str(), InsideCarTriggerInstructionScanResult - (std::uint8_t*)exeModule);

			uint32_t imm = *reinterpret_cast<uint32_t*>(InsideCarTriggerInstructionScanResult + 2);

			InsideCarTriggerValueAddress = reinterpret_cast<uint8_t*>(imm);

			static SafetyHookMid InsideCarTriggerInstructionMidHook{};

			InsideCarTriggerInstructionMidHook = safetyhook::create_mid(InsideCarTriggerInstructionScanResult, [](SafetyHookContext& ctx)
			{
				iInsideCar = *reinterpret_cast<uint32_t*>(InsideCarTriggerValueAddress);
			});
		}
		else
		{
			spdlog::error("Failed to locate inside car trigger instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 83 D0 00 00 00 D8 0D ?? ?? ?? ?? D9 F2 DD D8");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is EngineDll.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 6, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* WeaponZoomFOVInstruction1ScanResult = Memory::PatternScan(dllModule3, "D8 1D ?? ?? ?? ?? DF E0 F6 C4 01 74 0B");
		if (WeaponZoomFOVInstruction1ScanResult)
		{
			spdlog::info("Weapon Zoom FOV Instruction 1: Address is GameDll.dll+{:x}", WeaponZoomFOVInstruction1ScanResult - (std::uint8_t*)dllModule3);

			Memory::PatchBytes(WeaponZoomFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			WeaponZoomFOVInstruction1Hook = safetyhook::create_mid(WeaponZoomFOVInstruction1ScanResult + 6, WeaponZoomFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate weapon zoom FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* WeaponZoomFOVInstruction2ScanResult = Memory::PatternScan(dllModule3, "C7 41 2C 00 00 8C 42 C6 41 24 03 D9 41 2C 83 EC 08");
		if (WeaponZoomFOVInstruction2ScanResult)
		{
			spdlog::info("Weapon Zoom FOV Instruction 2: Address is GameDll.dll+{:x}", WeaponZoomFOVInstruction2ScanResult - (std::uint8_t*)dllModule3);

			Memory::Write(WeaponZoomFOVInstruction2ScanResult + 3, fNewWeaponZoomFOV);
		}
		else
		{
			spdlog::error("Failed to locate weapon zoom FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction1ScanResult = Memory::PatternScan(dllModule2, "D9 93 DC 00 00 00 D9 83 D4 00 00 00");
		if (AspectRatioInstruction1ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is EngineDll.dll+{:x}", AspectRatioInstruction1ScanResult + 6 - (std::uint8_t*)dllModule2);

			static SafetyHookMid AspectRatioInstruction1MidHook{};

			AspectRatioInstruction1MidHook = safetyhook::create_mid(AspectRatioInstruction1ScanResult + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.ebx + 0xD4) = 0.75f / fAspectRatioScale;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 1 memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstruction2ScanResult = Memory::PatternScan(dllModule2, "D9 83 D8 00 00 00 D9 C2 DE CA D9 C9 D8 F1");
		if (AspectRatioInstruction2ScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 2: Address is EngineDll.dll+{:x}", AspectRatioInstruction2ScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid AspectRatioInstruction2MidHook{};

			AspectRatioInstruction2MidHook = safetyhook::create_mid(AspectRatioInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.ebx + 0xD8) = 1.0f;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction 2 memory address.");
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