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
#include <locale>
#include <string>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "SearchAndRescueVietnamMedEvacWidescreenFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float tolerance = 0.0001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewCameraFOV;
float fNewAspectRatio;
float fFOVFactor;

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
	SARVME,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SARVME, {"Search & Rescue: Vietnam MedEvac", "Vr.exe"}},
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

// Function that writes an integer as a series of char8_t digits
void WriteIntAsChar8Digits(std::uint8_t* baseAddress, int value)
{
	std::string strValue = std::to_string(value);

	for (char ch : strValue)
	{
		char8_t char8Value = static_cast<char8_t>(ch);

		Memory::Write(baseAddress, char8Value);

		baseAddress += sizeof(char8_t);
	}
}

// Function that returns the amount of digits of a given number
int digitCount(int number)
{
	int count = 0;

	do
	{
		number /= 10;
		++count;
	} while (number != 0);

	return count;
}

float CalculateNewFOV(float fCurrentFOV)
{
	return fFOVFactor * (fCurrentFOV * (fNewAspectRatio / fOldAspectRatio));
}

void WidescreenFix()
{
	if (eGameType == Game::SARVME && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		// If both width and height values have 4 digits, then write the resolution values in place of the 1600x1200 one
		if (digitCount(iCurrentResX) == 4 && digitCount(iCurrentResY) == 4)
		{
			std::uint8_t* Resolution1600x1200ScanResult = Memory::PatternScan(exeModule, "31 36 30 30 20 78 20 31 32 30 30 20 78 20 33 32");
			if (Resolution1600x1200ScanResult)
			{
				spdlog::info("Resolution 1600x1200 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution1600x1200ScanResult - (std::uint8_t*)exeModule);

				WriteIntAsChar8Digits(Resolution1600x1200ScanResult, iCurrentResX);

				WriteIntAsChar8Digits(Resolution1600x1200ScanResult + 7, iCurrentResY);
			}
			else
			{
				spdlog::error("Failed to locate resolution 1600x1200 scan memory address.");
				return;
			}
		}

		// If width has 4 digits and height has 3 digits, then write the resolution values in place of the 1280x960 one
		if (digitCount(iCurrentResX) == 4 && digitCount(iCurrentResY) == 3)
		{
			std::uint8_t* Resolution1280x960ScanResult = Memory::PatternScan(exeModule, "31 32 38 30 20 78 20 39 36 30 20 78 20 33 32");
			if (Resolution1280x960ScanResult)
			{
				spdlog::info("Resolution 1280x960 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution1280x960ScanResult - (std::uint8_t*)exeModule);

				WriteIntAsChar8Digits(Resolution1280x960ScanResult, iCurrentResX);

				WriteIntAsChar8Digits(Resolution1280x960ScanResult + 7, iCurrentResY);
			}
			else
			{
				spdlog::error("Failed to locate resolution 1280x960 scan memory address.");
				return;
			}
		}

		// If both width and height values have 3 digits, then write the resolution values in place of the 800x600 one
		if (digitCount(iCurrentResX) == 3 && digitCount(iCurrentResY) == 3)
		{
			std::uint8_t* Resolution800x600ScanResult = Memory::PatternScan(exeModule, "38 30 30 20 78 20 36 30 30 20 78 20 33 32");
			if (Resolution800x600ScanResult)
			{
				spdlog::info("Resolution 800x600 Scan: Address is {:s}+{:x}", sExeName.c_str(), Resolution800x600ScanResult - (std::uint8_t*)exeModule);
				
				WriteIntAsChar8Digits(Resolution800x600ScanResult, iCurrentResX);

				WriteIntAsChar8Digits(Resolution800x600ScanResult + 6, iCurrentResY);
			}
			else
			{
				spdlog::error("Failed to locate resolution 800x600 scan memory address.");
				return;
			}
		}
		
		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 4E 68 8B 50 04 D8 76 68 89 56 6C 8B 46 04 85 C0");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};

			static std::vector<float> vComputedHFOVs;

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					// Store the value inside the ECX register into a float variable
					float fCurrentCameraHFOV = std::bit_cast<float>(ctx.ecx);

					// Skip processing if a similar HFOV (within tolerance) has already been computed
					bool alreadyComputed = std::any_of(vComputedHFOVs.begin(), vComputedHFOVs.end(),
						[&](float computedValue) {
							return std::fabs(computedValue - fCurrentCameraHFOV) < tolerance;
						});
					if (alreadyComputed)
					{
						return;
					}

					// Compute the new HFOV value
					fCurrentCameraHFOV = CalculateNewFOV(fCurrentCameraHFOV);

					// Record the computed HFOV for future calls
					vComputedHFOVs.push_back(fCurrentCameraHFOV);

					// Update the ECX register with the new HFOV value
					ctx.ecx = std::bit_cast<uintptr_t>(fCurrentCameraHFOV);
				});
		}
		else
		{
			spdlog::info("Cannot locate the camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 56 6C 8B 46 04 85 C0 D9 5E 70");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};

			static std::vector<float> vComputedVFOVs;

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					// Store the value inside the EDX register into a float variable
					float fCurrentCameraVFOV = std::bit_cast<float>(ctx.edx);

					// Skip processing if a similar VFOV (within tolerance) has already been computed
					bool alreadyComputed = std::any_of(vComputedVFOVs.begin(), vComputedVFOVs.end(),
						[&](float computedValue) {
							return std::fabs(computedValue - fCurrentCameraVFOV) < tolerance;
						});

					if (alreadyComputed)
					{
						return;
					}

					// Compute the new VFOV value
					fCurrentCameraVFOV = CalculateNewFOV(fCurrentCameraVFOV);

					// Record the computed VFOV for future calls
					vComputedVFOVs.push_back(fCurrentCameraVFOV);

					// Update the EDX register with the new VFOV value
					ctx.edx = std::bit_cast<uintptr_t>(fCurrentCameraVFOV);
				});
		}
		else
		{
			spdlog::info("Cannot locate the camera VFOV instruction memory address.");
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