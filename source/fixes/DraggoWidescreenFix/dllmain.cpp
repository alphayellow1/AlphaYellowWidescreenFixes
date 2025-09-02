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
std::string sFixName = "DraggoWidescreenFix";
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

// Variables
float fNewCameraFOV;
float fNewAspectRatio;
float fNewAspectRatio2;
float fAspectRatioScale;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
uint8_t* ResolutionWidth1Address;
uint8_t* ResolutionHeight1Address;
uint8_t* ResolutionWidth2Address;
uint8_t* ResolutionHeight2Address;
uint8_t* ResolutionWidth3Address;
uint8_t* ResolutionHeight3Address;
uint8_t* ResolutionWidth4Address;
uint8_t* ResolutionHeight4Address;
uint8_t* ResolutionWidth5Address;
uint8_t* ResolutionHeight5Address;
uint8_t* ResolutionWidth6Address;
uint8_t* ResolutionHeight6Address;
uint8_t* ResolutionWidth7Address;
uint8_t* ResolutionHeight7Address;
uint8_t* ResolutionWidth8Address;
uint8_t* ResolutionHeight8Address;
uint8_t* CameraFOV1Address;
uint8_t* CameraFOV2Address;

// Game detection
enum class Game
{
	DRAGGO,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DRAGGO, {"Draggo", "Draggo.exe"}},
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

static SafetyHookMid AspectRatioInstructionHook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		fld dword ptr ds:[fNewAspectRatio2]
	}
}

static SafetyHookMid CameraFOVInstruction1Hook{};

void CameraFOVInstruction1MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV1 = *reinterpret_cast<float*>(CameraFOV1Address);

	fNewCameraFOV1 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV1, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV1]
	}
}

static SafetyHookMid CameraFOVInstruction2Hook{};

void CameraFOVInstruction2MidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(CameraFOV2Address);

	fNewCameraFOV2 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV2, fAspectRatioScale) * fFOVFactor;

	_asm
	{
		fld dword ptr ds:[fNewCameraFOV2]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::DRAGGO && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructions1ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? A3 ?? ?? ?? ?? 0F BF 4E 06 89 0D ?? ?? ?? ?? 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions1ScanResult)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions1ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth1Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionWidth2Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 6, Memory::PointerMode::Absolute);

			ResolutionHeight1Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 16, Memory::PointerMode::Absolute);

			ResolutionHeight2Address = Memory::GetPointer<uint32_t>(ResolutionInstructions1ScanResult + 22, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions1ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10);

			static SafetyHookMid ResolutionWidthInstructions1MidHook{};

			ResolutionWidthInstructions1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth1Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionWidth2Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions1ScanResult + 14, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionHeightInstructions1MidHook{};

			ResolutionHeightInstructions1MidHook = safetyhook::create_mid(ResolutionInstructions1ScanResult + 14, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight1Address) = iCurrentResY;

				*reinterpret_cast<int*>(ResolutionHeight2Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 1 scan memory address.");
			return;
		}
		
		std::uint8_t* ResolutionInstructions2ScanResult = Memory::PatternScan(exeModule, "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 3D 40 01 00 00");
		if (ResolutionInstructions2ScanResult)
		{
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions2ScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(ResolutionInstructions2ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid ResolutionInstructions2MidHook{};

			ResolutionInstructions2MidHook = safetyhook::create_mid(ResolutionInstructions2ScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 2 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions3ScanResult = Memory::PatternScan(exeModule, "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 0F 84 ?? ?? ?? ??");
		if (ResolutionInstructions3ScanResult)
		{
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions3ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight4Address = Memory::GetPointer<uint32_t>(ResolutionInstructions3ScanResult + 8, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions3ScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions3MidHook{};

			ResolutionInstructions3MidHook = safetyhook::create_mid(ResolutionInstructions3ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth4Address) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeight4Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 3 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions4ScanResult = Memory::PatternScan(exeModule, "89 35 ?? ?? ?? ?? 8B 41 24 85 C0 75 15 8B 0D ?? ?? ?? ?? 33 C0 A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions4ScanResult)
		{
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions4ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth5Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight5Address = Memory::GetPointer<uint32_t>(ResolutionInstructions4ScanResult + 28, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions4ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction4MidHook{};

			ResolutionWidthInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth5Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions4ScanResult + 26, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction4MidHook{};

			ResolutionHeightInstruction4MidHook = safetyhook::create_mid(ResolutionInstructions4ScanResult + 26, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight5Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 4 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions5ScanResult = Memory::PatternScan(exeModule, "89 3D ?? ?? ?? ?? 89 35 ?? ?? ?? ?? 89 2D ?? ?? ?? ?? 7D 04 33 D2");
		if (ResolutionInstructions5ScanResult)
		{
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions5ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions5ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight6Address = Memory::GetPointer<uint32_t>(ResolutionInstructions5ScanResult + 14, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions5ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction5MidHook{};

			ResolutionWidthInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth6Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions5ScanResult + 12, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction5MidHook{};

			ResolutionHeightInstruction5MidHook = safetyhook::create_mid(ResolutionInstructions5ScanResult + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight6Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 5 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions6ScanResult = Memory::PatternScan(exeModule, "89 35 ?? ?? ?? ?? 5F 89 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions6ScanResult)
		{
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions6ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 2, Memory::PointerMode::Absolute);

			ResolutionHeight7Address = Memory::GetPointer<uint32_t>(ResolutionInstructions6ScanResult + 20, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions6ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionWidthInstruction6MidHook{};

			ResolutionWidthInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth7Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions6ScanResult + 18, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction6MidHook{};

			ResolutionHeightInstruction6MidHook = safetyhook::create_mid(ResolutionInstructions6ScanResult + 18, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight7Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 6 scan memory address.");
			return;
		}

		std::uint8_t* ResolutionInstructions7ScanResult = Memory::PatternScan(exeModule, "A3 ?? ?? ?? ?? 83 FD 10 0F BF 4E 06 89 0D ?? ?? ?? ??");
		if (ResolutionInstructions7ScanResult)
		{
			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructions7ScanResult - (std::uint8_t*)exeModule);

			ResolutionWidth8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions7ScanResult + 1, Memory::PointerMode::Absolute);

			ResolutionHeight8Address = Memory::GetPointer<uint32_t>(ResolutionInstructions7ScanResult + 14, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructions7ScanResult, "\x90\x90\x90\x90\x90", 5);

			static SafetyHookMid ResolutionWidthInstruction7MidHook{};

			ResolutionWidthInstruction7MidHook = safetyhook::create_mid(ResolutionInstructions7ScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidth8Address) = iCurrentResX;
			});

			Memory::PatchBytes(ResolutionInstructions7ScanResult + 12, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid ResolutionHeightInstruction7MidHook{};

			ResolutionHeightInstruction7MidHook = safetyhook::create_mid(ResolutionInstructions7ScanResult + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionHeight8Address) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions 7 scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 E1 D8 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D9 E1");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(AspectRatioInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			fNewAspectRatio2 = fAspectRatioScale;

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, AspectRatioInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction1ScanResult = Memory::PatternScan(exeModule, "D9 05 ?? ?? ?? ?? D9 E1 D9 15 ?? ?? ?? ?? DC 1D ?? ?? ?? ?? DF E0 F6 C4 05 7A 0C");
		if (CameraFOVInstruction1ScanResult)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction1ScanResult - (std::uint8_t*)exeModule);

			CameraFOV1Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction1ScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction1ScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstruction1ScanResult, CameraFOVInstruction1MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 1 memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstruction2ScanResult = Memory::PatternScan(exeModule, "74 5E D9 05 ?? ?? ?? ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 05 7A 1C D9 05 ?? ?? ?? ?? D9 E0");
		if (CameraFOVInstruction2ScanResult)
		{
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstruction2ScanResult + 2 - (std::uint8_t*)exeModule);

			CameraFOV2Address = Memory::GetPointer<uint32_t>(CameraFOVInstruction2ScanResult + 4, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstruction2ScanResult + 2, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstruction2ScanResult + 2, CameraFOVInstruction2MidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction 2 memory address.");
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