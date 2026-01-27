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

// Fix details
std::string sFixName = "BackyardSkateboardingWidescreenFix";
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
float fNewHUDHorizontalRes;
uint8_t* DX9CheckScanResult;
uint8_t* ResolutionInstructionsScanResult;
uint8_t* ResolutionInstructionsScanResult2;
uint8_t* CameraFOVInstructionScanResult;

// Game detection
enum class Game
{
	BS_DX9_ORIGINAL,
	BS_DX9_GOTY,
	BS_DX8_1,
	BS_DX8,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::BS_DX9_ORIGINAL, {"Backyard Skateboarding (DX9)", "BYSkateboarding.exe"}},
	{Game::BS_DX9_GOTY, {"Backyard Skateboarding GOTY (DX9)", "BYSkateboarding.exe"}},
	{Game::BS_DX8_1, {"Backyard Skateboarding (DX8.1)", "BYSkateboarding_DX8.1.exe"}},
	{Game::BS_DX8, {"Backyard Skateboarding (DX8)", "BYSkateboarding_DX8.exe"}},
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

Game DetectDX9Version()
{
	uint8_t* VersionCheckScanResult = Memory::PatternScan(exeModule, "?? ?? ?? ?? ?? 73 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? ?? E8 ?? ?? ?? ??");

	uint8_t* VersionCheckAddress = VersionCheckScanResult;

	uint8_t VersionCheckValue = *reinterpret_cast<uint8_t*>(VersionCheckAddress);

	if (VersionCheckValue == 0xBB)
	{
		return Game::BS_DX9_ORIGINAL;
	}		
	if (VersionCheckValue == 0xBE)
	{
		return Game::BS_DX9_GOTY;
	}
}

bool DetectGame()
{
	for (const auto& [type, info] : kGames)
	{
		if (!Util::stringcmp_caseless(info.ExeName, sExeName))
		{
			continue;
		}	

		eGameType = type;

		if (type == Game::BS_DX9_ORIGINAL || type == Game::BS_DX9_GOTY)
		{
			eGameType = DetectDX9Version();
		}

		if (eGameType == Game::Unknown)
		{
			spdlog::error("Failed to refine DX9 version");
			return false;
		}

		game = &kGames.at(eGameType);

		spdlog::info("Detected EXE: {} ({})", game->GameTitle, sExeName);

		spdlog::info("----------");
		return true;
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported.", sExeName);
	return false;
}

static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid HUDHorizontalResInstructionHook{};

void WidescreenFix()
{
	if ((eGameType == Game::BS_DX9_ORIGINAL || eGameType == Game::BS_DX9_GOTY || eGameType == Game::BS_DX8_1 || eGameType == Game::BS_DX8) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		if (eGameType == Game::BS_DX9_ORIGINAL || eGameType == Game::BS_DX8)
		{
			ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 74 ?? C7 44 24");
		}
		else if (eGameType == Game::BS_DX9_GOTY || eGameType == Game::BS_DX8_1)
		{
			ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 74 ?? 8B 08 50 FF 51 ?? 3B C7");
		}
		
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			if (eGameType == Game::BS_DX9_ORIGINAL || eGameType == Game::BS_DX8)
			{
				// 640x480
				Memory::Write(ResolutionInstructionsScanResult + 4, iCurrentResX);

				Memory::Write(ResolutionInstructionsScanResult + 12, iCurrentResY);
			}
			else if (eGameType == Game::BS_DX9_GOTY || eGameType == Game::BS_DX8_1)
			{
				// 640x480
				Memory::Write(ResolutionInstructionsScanResult + 4, iCurrentResX);

				Memory::Write(ResolutionInstructionsScanResult + 12, iCurrentResY);

				// 800x600
				Memory::Write(ResolutionInstructionsScanResult + 55, iCurrentResX);

				Memory::Write(ResolutionInstructionsScanResult + 63, iCurrentResY);
			}
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		if (eGameType == Game::BS_DX9_ORIGINAL || eGameType == Game::BS_DX8)
		{
			CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 46 ?? 83 C4 ?? 68");
		}
		else if (eGameType == Game::BS_DX9_GOTY || eGameType == Game::BS_DX8_1)
		{
			CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 4E ?? 83 C4 ?? 50 51");
		}

		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 3);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x2C);

				fNewCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

				if (eGameType == Game::BS_DX9_ORIGINAL || eGameType == Game::BS_DX8)
				{
					ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
				}
				else if (eGameType == Game::BS_DX9_GOTY || eGameType == Game::BS_DX8_1)
				{
					ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV);
				}				
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* HUDHorizontalResInstructionScanResult = Memory::PatternScan(exeModule, "8B 44 24 ?? 8B 4C 24 ?? 89 04 ?? 8B 44 24 ?? 8D 14");
		if (HUDHorizontalResInstructionScanResult)
		{
			spdlog::info("HUD Horizontal Res Instruction: Address is {:s}+{:x}", sExeName.c_str(), HUDHorizontalResInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(HUDHorizontalResInstructionScanResult, 4);

			HUDHorizontalResInstructionHook = safetyhook::create_mid(HUDHorizontalResInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDVerticalRes = *reinterpret_cast<float*>(ctx.esp + 0x14);

				fNewHUDHorizontalRes = fCurrentHUDVerticalRes * fNewAspectRatio;

				ctx.eax = std::bit_cast<uintptr_t>(fNewHUDHorizontalRes);
			});
		}
		else
		{
			spdlog::error("Failed to locate HUD horizontal resolution instruction memory address.");
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