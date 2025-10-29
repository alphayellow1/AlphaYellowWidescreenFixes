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
HMODULE dllModule2;

// Fix details
std::string sFixName = "StreetTennisTheNextGenerationChampionsWidescreenFix";
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
int iNewMatchResX;
int iNewMatchResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraHFOV;
float fNewCameraVFOV;

// Game detection
enum class Game
{
	STTNG,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::STTNG, {"Street Tennis: The Next Generation Champions", "Street.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "Width", iNewMatchResX);
	inipp::get_value(ini.sections["Settings"], "Height", iNewMatchResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iNewMatchResX);
	spdlog_confparse(iNewMatchResY);
	spdlog_confparse(fFOVFactor);

	// If resolution not specified, use desktop resolution
	if (iNewMatchResX <= 0 || iNewMatchResY <= 0)
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();
		iNewMatchResX = desktopDimensions.first;
		iNewMatchResY = desktopDimensions.second;
		spdlog_confparse(iNewMatchResX);
		spdlog_confparse(iNewMatchResY);
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

	while ((dllModule2 = GetModuleHandleA("3DGT.dll")) == nullptr)
	{
		spdlog::warn("3DGT.dll not loaded yet. Waiting...");
	}

	spdlog::info("Successfully obtained handle for 3DGT.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid CameraHFOVInstructionMidHook{};
static SafetyHookMid CameraVFOVInstructionMidHook{};

void WidescreenFix()
{
	if (eGameType == Game::STTNG && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iNewMatchResX) / static_cast<float>(iNewMatchResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 01 00 02 00 00 C7 41 04 80 01 00 00 C2 04 00 C7 01 80 02 00 00 C7 41 04 90 01 00 00 C2 04 00 C7 01 80 02 00 00 C7 41 04 E0 01 00 00 C2 04 00 C7 01 20 03 00 00 C7 41 04 58 02 00 00 C2 04 00 C7 01 00 04 00 00 C7 41 04 00 03 00 00 C2 04 00 C7 01 00 05 00 00 C7 41 04 00 04 00 00 C2 04 00 C7 01 40 06 00 00 C7 41 04 B0 04 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 512x384
			Memory::Write(ResolutionListScanResult + 2, iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 9, iNewMatchResY);

			// 640x400
			Memory::Write(ResolutionListScanResult + 18, iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 25, iNewMatchResY);

			// 640x480
			Memory::Write(ResolutionListScanResult + 34, iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 41, iNewMatchResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 50, iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 57, iNewMatchResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 66, iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 73, iNewMatchResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 82, iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 89, iNewMatchResY);

			// 1600x1200
			Memory::Write(ResolutionListScanResult + 98, iNewMatchResX);

			Memory::Write(ResolutionListScanResult + 105, iNewMatchResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list memory address.");
			return;
		}

		// Located in 3DGT.Camera::GetLens
		std::uint8_t* CameraFOVInstructionsScanResult = Memory::PatternScan(dllModule2, "8B 91 38 01 00 00 89 10 8B 89 3C 01 00 00");
		if (CameraFOVInstructionsScanResult)
		{
			spdlog::info("Camera HFOV Instruction Scan: Address is 3DGT.dll+{:x}", CameraFOVInstructionsScanResult - (std::uint8_t*)dllModule2);

			spdlog::info("Camera VFOV Instruction Scan: Address is 3DGT.dll+{:x}", CameraFOVInstructionsScanResult + 8 - (std::uint8_t*)dllModule2);

			Memory::PatchBytes(CameraFOVInstructionsScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = *reinterpret_cast<float*>(ctx.ecx + 0x138);

				fNewCameraHFOV = (fCurrentCameraHFOV / fAspectRatioScale) / fFOVFactor;

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraHFOV);
			});

			Memory::PatchBytes(CameraFOVInstructionsScanResult + 8, "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraFOVInstructionsScanResult + 8, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = *reinterpret_cast<float*>(ctx.ecx + 0x13C);

				fNewCameraVFOV = fCurrentCameraVFOV / fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraVFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instructions scan memory address.");
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