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
HMODULE dllModule2 = nullptr;
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "CrazySoccerMundialWidescreenFix";
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
constexpr float fOriginalMatchesFOV = 1.308996916f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fCurrentCameraFOV;
float fNewCameraHFOV;
float fNewCameraVFOV;
float fNewMatchesFOV;
int numHits;
std::vector<std::uint8_t*> CameraFOVInstructionsScanResult;

// Game detection
enum class Game
{
	CSM,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	HFOV,
	VFOV,
	MatchesFOV,
	hook4_1
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CSM, {"Crazy Soccer Mundial", "PetSoccer.bum"}},
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

	dllModule2 = Memory::GetHandle("ChromeEngine.dll", 50);

	dllModule3 = Memory::GetHandle("KernelGL.dll", 50);

	return true;
}

static SafetyHookMid ResolutionInstructionsHook{};
static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};
static SafetyHookMid InitializeHook{};

void WidescreenFix()
{
	if (eGameType == Game::CSM && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(dllModule2, "89 41 ?? 89 51 ?? C2 ?? ?? 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 8B 54 24");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is ChromeEngine.dll+{:x}", ResolutionInstructionsScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(ResolutionInstructionsScanResult, 6); // NOP out the original instructions			

			ResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0xC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.ecx + 0x10) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		CameraFOVInstructionsScanResult = Memory::PatternScan(dllModule2, "D8 4C 24 ?? D9 5C 24 ?? D8 C9", "D8 4C 24 ?? D9 5C 24 ?? DD D8", "B8 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 15", dllModule3, "d9 44 24 ?? d8 64 24 ?? da 35 ?? ?? ?? ?? d9 44 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScanResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is ChromeEngine.dll+{:x}", CameraFOVInstructionsScanResult[HFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera VFOV Instruction: Address is ChromeEngine.dll+{:x}", CameraFOVInstructionsScanResult[VFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Matches FOV Instruction: Address is ChromeEngine.dll+{:x}", CameraFOVInstructionsScanResult[MatchesFOV] - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(CameraFOVInstructionsScanResult[HFOV], 4); // NOP out the original instruction

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[HFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.esp + 0x8);

				fNewCameraHFOV = fCurrentCameraHFOV * fAspectRatioScale;

				FPU::FMUL(fNewCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult[VFOV], 4); // NOP out the original instruction

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[VFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.esp + 0x8);

				fNewCameraVFOV = fCurrentCameraVFOV * fAspectRatioScale;

				FPU::FMUL(fNewCameraVFOV);
			});

			fNewMatchesFOV = fOriginalMatchesFOV * fFOVFactor;
			
			numHits = 0;

			InitializeHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[hook4_1], [](SafetyHookContext& ctx)
			{
				numHits++;

				if (numHits == 20)
				{
					Memory::Write(CameraFOVInstructionsScanResult[MatchesFOV] + 1, fNewMatchesFOV);
				}				
			});
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