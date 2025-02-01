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

HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "EliteForcesNavySEALsWeaponsOfMassDestructionFOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float epsilon = 0.00001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;

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
		spdlog::info("Module Address: 0x{0:X}", (uintptr_t)dllModule);
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
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);

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
	Sleep(2000);

	dllModule = GetModuleHandleA("cshell.dll");
	if (!dllModule)
	{
		spdlog::error("Failed to get handle for cshell.dll.");
		return false;
	}

	spdlog::info("Successfully obtained handle for cshell.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule));

	return true;
}

void FOVFix()
{
	if (bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* HipfireCameraHFOVInstructionScanResult = Memory::PatternScan(dllModule, "8B 86 00 86 00 00 89 45 08");
		if (HipfireCameraHFOVInstructionScanResult)
		{
			spdlog::info("Hipfire Camera HFOV Instruction: Address is cshell.dll+{:x}", HipfireCameraHFOVInstructionScanResult - (std::uint8_t*)dllModule);

			static SafetyHookMid HipfireCameraHFOVInstructionMidHook{};
			HipfireCameraHFOVInstructionMidHook = safetyhook::create_mid(HipfireCameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (*reinterpret_cast<float*>(ctx.esi + 0x8600) == 1.0471975803375244f)
				{
					*reinterpret_cast<float*>(ctx.esi + 0x8600) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.esi + 0x8600) / 2.0f) * (fNewAspectRatio / fOldAspectRatio));
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire camera HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraZoomHFOVInstructionScanResult = Memory::PatternScan(dllModule, "89 86 FC 85 00 00 8D 86 78 87 00 00");
		if (CameraZoomHFOVInstructionScanResult)
		{
			spdlog::info("Camera Zoom HFOV Instruction: Address is cshell.dll+{:x}", CameraZoomHFOVInstructionScanResult - (std::uint8_t*)dllModule);

			static SafetyHookMid CameraZoomHFOVInstructionMidHook{};
			CameraZoomHFOVInstructionMidHook = safetyhook::create_mid(CameraZoomHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (ctx.eax == std::bit_cast<uint32_t>(0.5235987902f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(0.5235987902f / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
				}
				else if (ctx.eax == std::bit_cast<uint32_t>(0.174532935f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(0.174532935f / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
				}
				else if (ctx.eax == std::bit_cast<uint32_t>(0.06981316954f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(0.06981316954f / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera zoom HFOV instruction memory address.");
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