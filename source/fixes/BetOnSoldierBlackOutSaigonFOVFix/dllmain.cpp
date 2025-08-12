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
std::string sFixName = "BetOnSoldierBlackOutSaigonFOVFix";
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

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fTolerance = 0.0000001f;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fAspectRatioScale;
float fNewWeaponHipfireFOV;
float fNewWeaponZoomFOV;
float fNewSniperZoomFOV;
float fNewMinimapFOV;
static uint8_t* CameraZoomFOVAddress;
static uint8_t* CameraFOV2Address;
static uint8_t* HipfireCameraFOVAddress;
static uint8_t* CameraFOV4Address;
static uint8_t* CameraFOV5Address;
static uint8_t* CameraFOV6Address;

// Game detection
enum class Game
{
	BOSBOS,
	BOSBOSGAME,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BOSBOS, {"Bet on Soldier: Black-out Saigon", "BoS.exe"}},
	{Game::BOSBOSGAME, {"Bet on Soldier: Black-out Saigon", "BetOnSoldier_BlackOutSaigon.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("kte_core.dll")) == nullptr)
	{
		spdlog::warn("kte_core.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for kte_core.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	while ((dllModule3 = GetModuleHandleA("Bos.dll")) == nullptr)
	{
		spdlog::warn("Bos.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for Bos.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule3));

	while ((dllModule4 = GetModuleHandleA("kte_dx9.dll")) == nullptr)
	{
		spdlog::warn("kte_dx9.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for kte_dx9.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule4));

	return true;
}

static SafetyHookMid WeaponHipfireFOVInstructionHook{};

void WeaponHipfireFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponHipfireFOV = *reinterpret_cast<float*>(ctx.ecx + 0x3C);

	fNewWeaponHipfireFOV = Maths::CalculateNewFOV_RadBased(fCurrentWeaponHipfireFOV, fAspectRatioScale);

	_asm
	{
		fld dword ptr ds:[fNewWeaponHipfireFOV]
	}
}

static SafetyHookMid WeaponZoomFOVInstructionHook{};

void WeaponZoomFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentWeaponZoomFOV = *reinterpret_cast<float*>(ctx.ecx + 0x40);

	fNewWeaponZoomFOV = Maths::CalculateNewFOV_RadBased(fCurrentWeaponZoomFOV, fAspectRatioScale);

	_asm
	{
		fld dword ptr ds:[fNewWeaponZoomFOV]
	}
}

static SafetyHookMid SniperZoomFOVInstructionHook{};

void SniperZoomFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentSniperZoomFOV = *reinterpret_cast<float*>(ctx.edx);

	fNewSniperZoomFOV = Maths::CalculateNewFOV_RadBased(fCurrentSniperZoomFOV, fAspectRatioScale);

	_asm
	{
		fld dword ptr ds:[fNewSniperZoomFOV]
	}
}

void FOVFix()
{
	if ((eGameType == Game::BOSBOS || eGameType == Game::BOSBOSGAME) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "D8 B6 E0 00 00 00 D9 E8 D9 F3 D9 C0 D9 FF D9 5C 24 0C");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is kte_core.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);
			
			static SafetyHookMid AspectRatioInstructionMidHook{};

			AspectRatioInstructionMidHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.esi + 0xE0);

				if (fCurrentAspectRatio == 1.3333333333f)
				{
					fCurrentAspectRatio = fNewAspectRatio;
				}
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule3, "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 53 56 8B F1 8B 0D ?? ?? ?? ?? 89 44 24 08 89 4C 24 10 89 54 24 0C 74 1D A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ??");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is Bos.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule3);

			HipfireCameraFOVAddress = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 1, Memory::PointerMode::Absolute);

			CameraFOV2Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 7, Memory::PointerMode::Absolute);

			CameraZoomFOVAddress = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 17, Memory::PointerMode::Absolute);			

			CameraFOV4Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 36, Memory::PointerMode::Absolute);

			CameraFOV5Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 42, Memory::PointerMode::Absolute);

			CameraFOV6Address = Memory::GetPointer<uint32_t>(CameraFOVInstructionScanResult + 48, Memory::PointerMode::Absolute);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 11);

			static SafetyHookMid CameraHipfireAndMinimapFOVMidHook{};

			CameraHipfireAndMinimapFOVMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHipfireFOV = *reinterpret_cast<float*>(HipfireCameraFOVAddress);

				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(CameraFOV2Address);

				float fNewCameraHipfireFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraHipfireFOV, fAspectRatioScale) * fFOVFactor;

				float fNewCamera2FOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, fAspectRatioScale);

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraHipfireFOV);

				ctx.edx = std::bit_cast<uintptr_t>(fNewCamera2FOV);
			});

			Memory::PatchBytes(CameraFOVInstructionScanResult + 15, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraZoomFOVInstructionMidHook{};

			CameraZoomFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 15, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraZoomFOV = *reinterpret_cast<float*>(CameraZoomFOVAddress);

				float fNewCameraZoomFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraZoomFOV, fAspectRatioScale);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraZoomFOV);
			});			

			Memory::PatchBytes(CameraFOVInstructionScanResult + 35, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 17);

			static SafetyHookMid CameraFOV4MidHook{};

			CameraFOV4MidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 35, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV4 = *reinterpret_cast<float*>(CameraFOV4Address);

				float& fCurrentCameraFOV5 = *reinterpret_cast<float*>(CameraFOV5Address);

				float& fCurrentCameraFOV6 = *reinterpret_cast<float*>(CameraFOV6Address);

				float fNewCameraFOV4 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV4, fAspectRatioScale);

				float fNewCameraFOV5 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV5, fAspectRatioScale);

				float fNewCameraFOV6 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV6, fAspectRatioScale);

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV4);

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV5);

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV6);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* MinimapFOVInstructionScanResult = Memory::PatternScan(dllModule4, "8B 8E D4 00 00 00 52 50 51 8D 4C 24 2C FF 15 ?? ?? ?? ?? 8B 55 00 8D 44 24 1C");
		if (MinimapFOVInstructionScanResult)
		{
			spdlog::info("Minimap FOV Instruction: Address is kte_dx9.dll+{:x}", MinimapFOVInstructionScanResult - (std::uint8_t*)dllModule4);

			Memory::PatchBytes(MinimapFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid MinimapFOVInstructionMidHook{};

			MinimapFOVInstructionMidHook = safetyhook::create_mid(MinimapFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentMinimapFOV = *reinterpret_cast<float*>(ctx.esi + 0xD4);

				if (fCurrentMinimapFOV == 0.08638998121f || fCurrentMinimapFOV == 0.6283185482f)
				{
					fNewMinimapFOV = Maths::CalculateNewFOV_RadBased(fCurrentMinimapFOV, fAspectRatioScale);
				}
				else
				{
					fNewMinimapFOV = fCurrentMinimapFOV;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewMinimapFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the minimap FOV instruction memory address.");
			return;
		}

		std::uint8_t* WeaponFOVInstructionScanResult = Memory::PatternScan(dllModule3, "D9 41 40 C2 04 00 D9 41 3C C2 04 00 CC CC CC CC CC CC CC CC CC CC CC CC CC");
		if (WeaponFOVInstructionScanResult)
		{
			spdlog::info("Weapon FOV Instruction: Address is Bos.dll+{:x}", WeaponFOVInstructionScanResult - (std::uint8_t*)dllModule3);

			Memory::PatchBytes(WeaponFOVInstructionScanResult + 6, "\x90\x90\x90", 3);

			WeaponHipfireFOVInstructionHook = safetyhook::create_mid(WeaponFOVInstructionScanResult + 6, WeaponHipfireFOVInstructionMidHook);

			Memory::PatchBytes(WeaponFOVInstructionScanResult, "\x90\x90\x90", 3);

			WeaponZoomFOVInstructionHook = safetyhook::create_mid(WeaponFOVInstructionScanResult, WeaponZoomFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the weapon FOV instruction memory address.");
			return;
		}

		std::uint8_t* SniperZoomFOVInstructionScanResult = Memory::PatternScan(dllModule3, "D9 02 8D 04 88 C2 04 00 CC CC CC CC CC CC CC CC CC CC CC CC CC CC");
		if (SniperZoomFOVInstructionScanResult)
		{
			spdlog::info("Sniper Zoom FOV Instruction: Address is Bos.dll+{:x}", SniperZoomFOVInstructionScanResult - (std::uint8_t*)dllModule3);
			
			Memory::PatchBytes(SniperZoomFOVInstructionScanResult, "\x90\x90", 2);
			
			SniperZoomFOVInstructionHook = safetyhook::create_mid(SniperZoomFOVInstructionScanResult, SniperZoomFOVInstructionMidHook);
		}
		else
		{
			spdlog::info("Cannot locate the sniper zoom FOV instruction memory address.");
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