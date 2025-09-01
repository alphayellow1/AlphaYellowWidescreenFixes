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
HMODULE dllModule2;
HMODULE thisModule;

// Fix details
std::string sFixName = "Hidden&DangerousDeluxeFOVFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// New INI variables
float fFOVFactor;
float fRenderingDistanceFactor;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;
float fAspectRatioScale;
float fNewAspectRatio;
float fNewAspectRatio2;
float fNewRenderingDistance;
float fNewRenderingDistance2;
float fCurrentCameraFOV;

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
		spdlog::info("Module Address: 0x{:X}", (uintptr_t)exeModule);
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load new INI entries
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	inipp::get_value(ini.sections["Settings"], "RenderingDistanceFactor", fRenderingDistanceFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fRenderingDistanceFactor);

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

	while ((dllModule2 = GetModuleHandleA("i3d2.dll")) == nullptr)
	{
		spdlog::warn("i3d2.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for i3d2.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ebx + 0x1E4);

	if (fCurrentCameraFOV == 1.483529806f)
	{
		fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x1E4);

	if (fCurrentCameraFOV2 == 1.483529806f)
	{
		fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

static SafetyHookMid CameraFOVInstruction3Hook{};

void CameraFOVInstruction3MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV3 = *reinterpret_cast<float*>(ctx.eax + 0x1E4);

	if (fCurrentCameraFOV3 == 1.483529806f)
	{
		fNewCameraFOV3 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV3, fAspectRatioScale) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV3 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV3, fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV3]
	}
}

static SafetyHookMid CameraFOVInstruction4Hook{};

void CameraFOVInstruction4MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV4 = *reinterpret_cast<float*>(ctx.ecx + 0x1E4);

	if (fCurrentCameraFOV4 == 1.483529806f)
	{
		fNewCameraFOV4 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV4, fAspectRatioScale) * fFOVFactor;
	}
	else
	{
		fNewCameraFOV4 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV4, fAspectRatioScale);
	}

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV4]
	}
}

static SafetyHookMid RenderingDistanceInstructionHook{};

void RenderingDistanceInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentRenderingDistance = *reinterpret_cast<float*>(ctx.ebx + 0x1EC);

	if (fCurrentCameraFOV == 1.483529806f && fCurrentRenderingDistance == 50.0f)
	{
		fNewRenderingDistance = fCurrentRenderingDistance * fRenderingDistanceFactor;
	}
	else
	{
		fNewRenderingDistance = fCurrentRenderingDistance;
	}

	_asm
	{
		fld dword ptr ds:[fNewRenderingDistance]
	}
}

static SafetyHookMid RenderingDistanceInstruction2Hook{};

void RenderingDistanceInstruction2MidHook(SafetyHookContext& ctx)
{
	
	
}

void FOVFix()
{
	if (eGameType == Game::HDD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "C7 80 9C 02 00 00 00 00 40 3F 83 E1 FD DD D8 89 48 40 8D 88 7C 02 00 00");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is i3d2.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			Memory::Write(AspectRatioInstructionScanResult + 6, fNewAspectRatio2);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 83 E4 01 00 00 D8 0D ?? ?? ?? ?? D9 C0 D9 FF D9 5D FC D9 FE");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is i3d2.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(dllModule2, "D9 80 E4 01 00 00 D8 0D ?? ?? ?? ?? 89 8D 7C FF FF FF D9 C0 D9 FE D9 5D C0 D9 FF");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is i3d2.dll+{:x}", CameraFOVInstruction2ScanResult - (std::uint8_t*)dllModule2);
			
			Memory::PatchBytes(CameraFOVInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction3ScanResult = Memory::PatternScan(dllModule2, "D9 80 E4 01 00 00 8B 17 D9 1C 24 57 FF 92 C8 00 00 00 8B 8E 9C 02 00 00 51 8B CF");
		if (CameraFOVInstruction3ScanResult)
		{
			spdlog::info("Camera FOV Instruction 3: Address is i3d2.dll+{:x}", CameraFOVInstruction3ScanResult - (std::uint8_t*)dllModule2);
			
			Memory::PatchBytes(CameraFOVInstruction3ScanResult, "\x90\x90\x90\x90\x90\x90", 6);
			
			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstruction3ScanResult, CameraFOVInstruction3MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 3 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction4ScanResult = Memory::PatternScan(dllModule2, "D9 81 E4 01 00 00 D8 0D ?? ?? ?? ?? D9 53 7C 8B 50 38 D8 8A F8 02 00 00 D9 5B 7C 5B");
		if (CameraFOVInstruction4ScanResult)
		{
			spdlog::info("Camera FOV Instruction 4: Address is i3d2.dll+{:x}", CameraFOVInstruction4ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstruction4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstruction4ScanResult, CameraFOVInstruction4MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 4 memory address.");
			return;
		}

		std::uint8_t* RenderingDistanceInstructionScanResult = Memory::PatternScan(dllModule2, "D9 FE D9 83 EC 01 00 00 D8 A3 E8 01 00 00");
		if (RenderingDistanceInstructionScanResult)
		{
			spdlog::info("Rendering Distance Instruction: Address is i3d2.dll+{:x}", RenderingDistanceInstructionScanResult + 2 - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(RenderingDistanceInstructionScanResult + 2, "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			RenderingDistanceInstructionHook = safetyhook::create_mid(RenderingDistanceInstructionScanResult + 2, RenderingDistanceInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate rendering distance instruction memory address.");
			return;
		}

		std::uint8_t* RenderingDistanceInstruction2ScanResult = Memory::PatternScan(dllModule2, "8B 90 EC 01 00 00 8D 98 8C 00 00 00 89 55 C4 8A 51 40 F6 C2 02 89 5D F4 8D 73 30");
		if (RenderingDistanceInstruction2ScanResult)
		{
			spdlog::info("Rendering Distance Instruction 2: Address is i3d2.dll+{:x}", RenderingDistanceInstruction2ScanResult - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(RenderingDistanceInstruction2ScanResult, "\x90\x90\x90\x90\x90\x90", 6); // NOP out the original instruction

			RenderingDistanceInstruction2Hook = safetyhook::create_mid(RenderingDistanceInstruction2ScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentRenderingDistance2 = *reinterpret_cast<float*>(ctx.eax + 0x1EC);

				if (fCurrentCameraFOV == 1.483529806f && fCurrentRenderingDistance2 == 50.0f)
				{
					fNewRenderingDistance2 = fCurrentRenderingDistance2 * fRenderingDistanceFactor;
				}
				else
				{
					fNewRenderingDistance2 = fCurrentRenderingDistance2;
				}
					
				ctx.edx = std::bit_cast<uintptr_t>(fNewRenderingDistance2);
			});
		}
		else
		{
			spdlog::error("Failed to locate rendering distance instruction 2 memory address.");
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