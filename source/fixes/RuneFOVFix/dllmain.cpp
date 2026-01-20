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
HMODULE EngineDLLHandle = nullptr;
HMODULE D3D7DLLHandle = nullptr;
HMODULE D3D9DLLHandle = nullptr;
HMODULE OpenGLDLLHandle = nullptr;

// Fix details
std::string sFixName = "RuneFOVFix";
std::string sFixVersion = "1.3";
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
float fNewAspectRatio;
float fAspectRatioScale;
float fCurrentCameraFOV1;
float fCurrentCameraFOV2;
float fCurrentCameraFOV3;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;

// Game detection
enum class Game
{
	RUNE,
	Unknown
};

enum CameraFOVInstructionsIndex
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::RUNE, {"Rune", "Rune.exe"}},
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

	EngineDLLHandle = Memory::GetHandle("Engine.dll", 100, 5);

	D3D7DLLHandle = Memory::GetHandle("D3DDrv.dll", 100, 5);

	D3D9DLLHandle = Memory::GetHandle("D3D9Drv.dll", 100, 5);

	OpenGLDLLHandle = Memory::GetHandle("OpenGLDrv.dll", 100, 5);

	return true;
}

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};

void FOVFix()
{
	if (eGameType == Game::RUNE && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult;

		if (D3D7DLLHandle)
		{
			CameraFOVInstructionsScansResult = Memory::PatternScan(EngineDLLHandle, "D9 82 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DD 1C", "");
		}
		else if (D3D9DLLHandle)
		{
			CameraFOVInstructionsScansResult = Memory::PatternScan(EngineDLLHandle, "D9 82 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DD 1C", D3D9DLLHandle, "D9 81 ?? ?? ?? ?? DC 0D");
		}
		else if (OpenGLDLLHandle)
		{
			CameraFOVInstructionsScansResult = Memory::PatternScan(EngineDLLHandle, "D9 82 ?? ?? ?? ?? DC 0D ?? ?? ?? ?? DD 1C", OpenGLDLLHandle, "D9 80 ?? ?? ?? ?? DC 0D");
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			if (D3D9DLLHandle || OpenGLDLLHandle)
			{
				spdlog::info("Camera FOV Instruction 1: Address is Engine.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)EngineDLLHandle);
			}
			else
			{
				spdlog::info("Camera FOV Instruction: Address is Engine.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)EngineDLLHandle);
			}
			
			if (D3D9DLLHandle)
			{
				spdlog::info("Camera FOV Instruction 2: Address is D3D9Drv.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)D3D9DLLHandle);
			}
			else if (OpenGLDLLHandle)
			{
				spdlog::info("Camera FOV Instruction 2: Address is OpenGLDrv.dll+{:x}", CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)OpenGLDLLHandle);
			}

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CameraFOV1Scan], 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.edx + 0x550);

				if (fCurrentCameraFOV1 != fNewCameraFOV1)
				{
					fNewCameraFOV1 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV1, fAspectRatioScale) * fFOVFactor;
				}

				FPU::FLD(fNewCameraFOV1);
			});

			if (D3D9DLLHandle || OpenGLDLLHandle)
			{
				Memory::WriteNOPs(CameraFOVInstructionsScansResult[CameraFOV2Scan], 6);

				CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], [](SafetyHookContext& ctx)
				{
					if (D3D9DLLHandle)
					{
						fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.ecx + 0x550);
					}
					else
					{
						fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.eax + 0x550);
					}					

					if (fCurrentCameraFOV2 != fNewCameraFOV2)
					{
						fNewCameraFOV2 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV2, fAspectRatioScale) * fFOVFactor;
					}

					FPU::FLD(fNewCameraFOV2);
				});
			}
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