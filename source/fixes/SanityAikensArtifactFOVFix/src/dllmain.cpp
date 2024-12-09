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
#include <cmath> // For atan, tan
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE dllModule = nullptr;
HMODULE thisModule;

// Fix details
std::string sFixName = "SanityAikensArtifactFOVFix";
std::string sFixVersion = "1.5"; // Updated version
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixFOV = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

const float epsilon = 0.00001f;

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
	inipp::get_value(ini.sections["FOV"], "Enabled", FixFOV);
	spdlog_confparse(FixFOV);

	// Load resolution from ini
	inipp::get_value(ini.sections["Resolution"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Resolution"], "Height", iCurrentResY);
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

void OpenGame()
{
	dllModule = GetModuleHandleA("client.dll");

	while (!dllModule)
	{
		spdlog::warn("Waiting for client.dll to load...");
		Sleep(1000); // Delay to wait for the DLL to load
	}

	spdlog::info("Successfully obtained handle for client.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule));
}

void FOV()
{
	std::uint8_t* SAA_HFOVScanResult = Memory::PatternScan(dllModule, "8B B0 54 01 00 00 89 B4 24 D0 00 00 00");
	if (SAA_HFOVScanResult) {
		spdlog::info("HFOV: Address is client.dll+{:x}", SAA_HFOVScanResult - (std::uint8_t*)dllModule);
		static SafetyHookMid SAA_HFOVMidHook{};
		SAA_HFOVMidHook = safetyhook::create_mid(SAA_HFOVScanResult,
			[](SafetyHookContext& ctx) {
			if (*reinterpret_cast<float*>(ctx.eax + 0x154) == 1.5707963705062866f)
			{
				*reinterpret_cast<float*>(ctx.eax + 0x154) = 2.0f * atanf(tanf(1.5707963705062866f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / oldAspectRatio));
			}
		});
	}

	std::uint8_t* SAA_VFOVScanResult = Memory::PatternScan(dllModule, "8B B0 58 01 00 00 89 B4 24 D4 00 00 00");
	if (SAA_VFOVScanResult) {
		spdlog::info("VFOV: Address is client.dll+{:x}", SAA_VFOVScanResult - (std::uint8_t*)dllModule);
		static SafetyHookMid SAA_VFOVMidHook{};
		SAA_VFOVMidHook = safetyhook::create_mid(SAA_VFOVScanResult,
			[](SafetyHookContext& ctx) {
			if (fabs(*reinterpret_cast<float*>(ctx.eax + 0x158) - (1.1780972480773926f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon)
			{
				*reinterpret_cast<float*>(ctx.eax + 0x158) = 1.1780972480773926f;
			}
			else if (*reinterpret_cast<float*>(ctx.eax + 0x158) == 1.5707963705062866f)
			{
				*reinterpret_cast<float*>(ctx.eax + 0x158) = 1.5707963705062866f;
			}
		});
	}
}

DWORD __stdcall Main(void*)
{
	Logging();
	Configuration();
	OpenGame();
	FOV();
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