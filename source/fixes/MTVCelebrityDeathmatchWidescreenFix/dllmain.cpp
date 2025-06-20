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
#include <cmath>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule = nullptr;

// Fix details
std::string sFixName = "MTVCelebrityDeathmatchWidescreenFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;
float fNewCameraFOVProjection;
float fNewCameraFOV;
float fNewCameraFOV2;
float fAspectRatioScale;
static uint8_t* CameraFOVValueAddress = nullptr;

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
	MTVCDGAME,
	MTVCD_EU_EN,
	MTVCD_FR,
	MTVCD_GE,
	MTVCD_IT,
	MTVCD_SP,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MTVCDGAME, {"MTV Celebrity Deathmatch", "cdm_dx.exe"}},
	{Game::MTVCD_EU_EN, {"MTV Celebrity Deathmatch", "cdm_dx_eu_en.exe"}},
	{Game::MTVCD_FR, {"MTV Celebrity Deathmatch", "cdm_dx_fr.exe"}},
	{Game::MTVCD_GE, {"MTV Celebrity Deathmatch", "cdm_dx_ge.exe"}},
	{Game::MTVCD_IT, {"MTV Celebrity Deathmatch", "cdm_dx_it.exe"}},
	{Game::MTVCD_SP, {"MTV Celebrity Deathmatch", "cdm_dx_sp.exe"}},
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
	return 2.0f * RadToDeg(atanf(tanf(DegToRad(fCurrentFOV / 2.0f)) * fAspectRatioScale));
}

void FOVFix()
{
	if ((eGameType == Game::MTVCDGAME || eGameType == Game::MTVCD_EU_EN || eGameType == Game::MTVCD_FR || eGameType == Game::MTVCD_GE || eGameType == Game::MTVCD_IT || eGameType == Game::MTVCD_SP) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 01 20 03 00 00 C7 02 58 02 00 00 C7 00 10 00 00 00 C3 8B 4C 24 04 8B 54 24 08 8B 44 24 0C C7 01 00 04 00 00 C7 02 00 03 00 00 C7 00 10 00 00 00 C3 8B 4C 24 04 8B 54 24 08 8B 44 24 0C C7 01 00 05 00 00 C7 02 C0 03 00 00 C7 00 10 00 00 00 C3 8B 4C 24 04 8B 54 24 08 8B 44 24 0C C7 01 80 02 00 00 C7 02 E0 01 00 00 C7 00 18 00 00 00 C3 8B 4C 24 04 8B 54 24 08 8B 44 24 0C C7 01 20 03 00 00 C7 02 58 02 00 00 C7 00 18 00 00 00 C3 8B 4C 24 04 8B 54 24 08 8B 44 24 0C C7 01 00 04 00 00 C7 02 00 03 00 00 C7 00 18 00 00 00 C3 8B 4C 24 04 8B 54 24 08 8B 44 24 0C C7 01 00 05 00 00 C7 02 C0 03 00 00 C7 00 18 00 00 00 C3 8B 4C 24 04 8B 54 24 08 8B 44 24 0C C7 01 80 02 00 00 C7 02 E0 01 00 00 C7 00 10 00 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionListScanResult + 2, iCurrentResX); // 800

			Memory::Write(ResolutionListScanResult + 8, iCurrentResY); // 600

			Memory::Write(ResolutionListScanResult + 33, iCurrentResX); // 1024

			Memory::Write(ResolutionListScanResult + 39, iCurrentResY); // 768

			Memory::Write(ResolutionListScanResult + 64, iCurrentResX); // 1280

			Memory::Write(ResolutionListScanResult + 70, iCurrentResY); // 960

			Memory::Write(ResolutionListScanResult + 95, iCurrentResX); // 640

			Memory::Write(ResolutionListScanResult + 101, iCurrentResY); // 480

			Memory::Write(ResolutionListScanResult + 126, iCurrentResX); // 800

			Memory::Write(ResolutionListScanResult + 132, iCurrentResY); // 600

			Memory::Write(ResolutionListScanResult + 157, iCurrentResX); // 1024

			Memory::Write(ResolutionListScanResult + 163, iCurrentResY); // 768

			Memory::Write(ResolutionListScanResult + 188, iCurrentResX); // 1280

			Memory::Write(ResolutionListScanResult + 194, iCurrentResY); // 960

			Memory::Write(ResolutionListScanResult + 219, iCurrentResX); // 640

			Memory::Write(ResolutionListScanResult + 225, iCurrentResY); // 480
		}
		else
		{
			spdlog::error("Failed to locate resolution list memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 0D ?? ?? ?? ?? D9 1C 24 68 00 00 80 BF 51");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			uint32_t imm = *reinterpret_cast<uint32_t*>(CameraFOVInstructionScanResult + 2);
			
			CameraFOVValueAddress = reinterpret_cast<uint8_t*>(imm);

			spdlog::info("Camera FOV value address: 0x{:X}", reinterpret_cast<uintptr_t>(CameraFOVValueAddress));

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			static SafetyHookMid CameraFOVInstructionMidHook{};

			CameraFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionScanResult + 6, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVValueAddress);

				if (fCurrentCameraFOV == 60.0f)
				{
					fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV) * fFOVFactor;
				}
				else
				{
					fNewCameraFOV = CalculateNewFOV(fCurrentCameraFOV);
				}

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			});
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