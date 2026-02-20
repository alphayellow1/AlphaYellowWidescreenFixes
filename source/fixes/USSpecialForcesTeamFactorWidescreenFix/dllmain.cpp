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
std::string sFixName = "USSpecialForcesTeamFactorWidescreenFix";
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
float fNewAspectRatio;
float fAspectRatioScale;
double dNewAspectRatio;
float fNewCameraFOV;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	USSFTF_GAME,
	USSFTF_LAUNCHER,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Resolution1Scan,
	Resolution2Scan,
	Resolution3Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::USSFTF_GAME, {"US Special Forces: Team Factor", "tf.exe"}},
	{Game::USSFTF_LAUNCHER, {"US Special Forces: Team Factor - Launcher", "TFLoader.exe"}},
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
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::USSFTF_LAUNCHER)
		{
			dllModule2 = Memory::GetHandle({"TFLoader-en.dll", "TFLoader-fr.dll", "TFLoader-ge.dll"});

			std::string sDllName = Memory::GetModuleName(dllModule2);
			
			std::uint8_t* ResolutionStringScanResult = Memory::PatternScan(dllModule2, "31 32 38 30 20 78 20 31 30 32 34");
			if (ResolutionStringScanResult)
			{
				spdlog::info("Resolution String Scan: Address is {}+{:x}", sDllName, ResolutionStringScanResult - (std::uint8_t*)dllModule2);

				if (Maths::digitCount(iCurrentResX) == 4 && Maths::digitCount(iCurrentResY) == 4)
				{
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, iCurrentResY);
				}
				
				if (Maths::digitCount(iCurrentResX) == 4 && Maths::digitCount(iCurrentResY) == 3)
				{
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, iCurrentResY);

					Memory::PatchBytes(ResolutionStringScanResult + 10, "\x00");
				}

				if (Maths::digitCount(iCurrentResX) == 3 && Maths::digitCount(iCurrentResY) == 3)
				{
					Memory::PatchBytes(ResolutionStringScanResult, "\x00");

					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 1, iCurrentResX);

					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, iCurrentResY);

					Memory::PatchBytes(ResolutionStringScanResult + 10, "\x00");
				}
			}
			else
			{
				spdlog::error("Failed to locate resolution string memory address.");
				return;
			}
		}

		if (eGameType == Game::USSFTF_GAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "00 05 00 00 00 04 00 00 DB 0F 49 40 DB 0F 49 40 DB 0F", "00 05 00 00 00 04 00 00 90 A2 4A 00 B0 A2 4A 00", "00 05 00 00 BE 00 04 00 00 EB 0A BD 80");
			if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
			{
				spdlog::info("Resolution 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution1Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution2Scan] - (std::uint8_t*)exeModule);

				spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Resolution3Scan] - (std::uint8_t*)exeModule);
								
				Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan], iCurrentResX);

				Memory::Write(ResolutionInstructionsScansResult[Resolution1Scan] + 4, iCurrentResY);

				Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan], iCurrentResX);

				Memory::Write(ResolutionInstructionsScansResult[Resolution2Scan] + 4, iCurrentResY);

				Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan], iCurrentResX);

				Memory::Write(ResolutionInstructionsScansResult[Resolution3Scan] + 5, iCurrentResY);
			}

			std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(exeModule, "DC 3D ?? ?? ?? ?? D9 5C 24");
			if (AspectRatioInstructionScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(AspectRatioInstructionScanResult, 6);

				dNewAspectRatio = 1.0 / (double)fAspectRatioScale;

				AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
				{
					FPU::FDIVR(dNewAspectRatio);
				});
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction memory address.");
				return;
			}

			std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D8 0D ?? ?? ?? ?? D8 05 ?? ?? ?? ?? D9 5C 24 ?? D9 44 24");
			if (CameraFOVInstructionScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

				CameraFOVAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

				Memory::WriteNOPs(CameraFOVInstructionScanResult, 6);

				CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV = *reinterpret_cast<float*>(CameraFOVAddress);

					fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

					FPU::FMUL(fNewCameraFOV);
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction memory address.");
				return;
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