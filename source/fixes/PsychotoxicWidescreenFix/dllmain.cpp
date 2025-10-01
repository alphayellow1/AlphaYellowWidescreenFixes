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
HMODULE dllModule3 = nullptr;
HMODULE dllModule4 = nullptr;

// Fix details
std::string sFixName = "PsychotoxicWidescreenFix";
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
float fNewCameraHFOV;
int iNewCameraFOV;
uint8_t* ResolutionWidth6Address;
uint8_t* ResolutionHeight6Address;
uint8_t* ResolutionWidth7Address;
uint8_t* ResolutionHeight7Address;
uint8_t* CameraHFOVAddress;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	PSYCHOTOXIC,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Resolution1Scan,
	Resolution2Scan,
	Resolution3Scan,
	Resolution4Scan,
	Resolution5Scan,
	Resolution6Scan,
	Resolution7Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PSYCHOTOXIC, {"Psychotoxic", "Psychotoxic.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("vision.dll")) == nullptr)
	{
		spdlog::warn("vision.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for vision.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while ((dllModule3 = GetModuleHandleA("vBase.dll")) == nullptr)
	{
		spdlog::warn("vBase.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for vBase.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	while ((dllModule4 = GetModuleHandleA("PTBase.dll")) == nullptr)
	{
		spdlog::warn("PTBase.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for PTBase.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule4));

	return true;
}

static SafetyHookMid CameraHFOVInstructionHook{};

void CameraHFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV = *reinterpret_cast<float*>(CameraHFOVAddress);

	fNewCameraHFOV = fCurrentCameraHFOV / fAspectRatioScale;

	_asm
	{
		fld dword ptr ds:[fNewCameraHFOV]
	}
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	int& iCurrentCameraFOV = *reinterpret_cast<int*>(CameraFOVAddress);

	iNewCameraFOV = (int)(iCurrentCameraFOV / fFOVFactor);

	_asm
	{
		fild dword ptr ds:[iNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::PSYCHOTOXIC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(dllModule2, "8B 44 24 ?? 56 8B F1 8B 4C 24 ?? 57 89 56", "8B 5C 24 08 56 8B 74 24 14 57 8D 04 16 3B C3 7E 08 5F 5E 32 C0 5B C2 18 00 8B 4C 24 1C 8B 44 24 24 8B 7C 24 14",
			dllModule3, "8B 4C 24 ?? 8B 54 24 ?? 89 0D", "8B 0E 89 0D ?? ?? ?? ?? 8B 56 04", "8B 0E 89 0D ?? ?? ?? ?? 8B 7E 04",
			dllModule4, "89 3D ?? ?? ?? ?? 89 35", "A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 57 A3 ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is vision.dll+{:x}", ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Resolution Instructions 2 Scan: Address is vision.dll+{:x}", ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)dllModule2);

			spdlog::info("Resolution Instructions 3 Scan: Address is vBase.dll+{:x}", ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)dllModule3);

			spdlog::info("Resolution Instructions 4 Scan: Address is vBase.dll+{:x}", ResolutionInstructionsScansResult[Resolution4Scan] - (std::uint8_t*)dllModule3);

			spdlog::info("Resolution Instructions 5 Scan: Address is vBase.dll+{:x}", ResolutionInstructionsScansResult[Resolution5Scan] - (std::uint8_t*)dllModule3);

			spdlog::info("Resolution Instructions 6 Scan: Address is PTBase.dll+{:x}", ResolutionInstructionsScansResult[Resolution6Scan] - (std::uint8_t*)dllModule4);

			spdlog::info("Resolution Instructions 7 Scan: Address is PTBase.dll+{:x}", ResolutionInstructionsScansResult[Resolution7Scan] - (std::uint8_t*)dllModule4);

			ResolutionWidth6Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScansResult[Resolution6Scan] + 2, Memory::PointerMode::Absolute);

			ResolutionHeight6Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScansResult[Resolution6Scan] + 8, Memory::PointerMode::Absolute);

			ResolutionWidth7Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScansResult[Resolution7Scan] + 1, Memory::PointerMode::Absolute);

			ResolutionHeight7Address = Memory::GetPointer<uint32_t>(ResolutionInstructionsScansResult[Resolution7Scan] + 17, Memory::PointerMode::Absolute);

			/*
			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan], "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionWidthInstruction1MidHook{};

			ResolutionWidthInstruction1MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution1Scan] + 7, "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionHeightInstruction1MidHook{};

			ResolutionHeightInstruction1MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution1Scan] + 7, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan], "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionWidthInstruction2MidHook{};

			ResolutionWidthInstruction2MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan], [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan] + 5, "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionWidthInstruction3MidHook{};

			ResolutionWidthInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan] + 5, [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan] + 33, "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionHeightInstruction2MidHook{};

			ResolutionHeightInstruction2MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan] + 33, [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution2Scan] + 25, "\x90\x90\x90\x90", 4);

			static SafetyHookMid ResolutionHeightInstruction3MidHook{};

			ResolutionHeightInstruction3MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution2Scan] + 25, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			
			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution3Scan], "\x90\x90\x90\x90\x90\x90\x90\x90", 8);

			static SafetyHookMid ResolutionInstructions4MidHook{};

			ResolutionInstructions4MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution3Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan], "\x90\x90", 2);

			static SafetyHookMid ResolutionWidthInstruction4MidHook{};

			ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution4Scan] + 8, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction4MidHook{};

			ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution4Scan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan], "\x90\x90", 2);

			static SafetyHookMid ResolutionWidthInstruction5MidHook{};

			ResolutionWidthInstruction5MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution5Scan] + 8, "\x90\x90\x90", 3);

			static SafetyHookMid ResolutionHeightInstruction5MidHook{};

			ResolutionHeightInstruction5MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution5Scan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(iCurrentResY);
			});
			*/

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution6Scan], "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions6MidHook{};

			ResolutionInstructions6MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution6Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth6Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight6Address) = iCurrentResY;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution7Scan], "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction7MidHook{};

			ResolutionWidthInstruction7MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution7Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth7Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructionsScansResult[Resolution7Scan] + 16, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionHeightInstruction7MidHook{};

			ResolutionHeightInstruction7MidHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Resolution7Scan] + 16, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight7Address) = iCurrentResY;
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "DB 05 ?? ?? ?? ?? DB 05 ?? ?? ?? ?? D9 05");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is vision.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			CameraFOVAddress = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 05 ?? ?? ?? ?? D8 C9 D8 CA");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is vision.dll+{:x}", CameraHFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			CameraHFOVAddress = Memory::GetPointer<uint32_t>(CameraHFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraHFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, CameraHFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction memory address.");
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