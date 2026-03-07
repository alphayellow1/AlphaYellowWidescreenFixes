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
HMODULE dllModule2 = nullptr;
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "EuroCopsFOVFix";
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

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fZoomFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewGeneralFOV;
float fNewHipfireFOV;
float fNewZoomFOV;

// Game detection
enum class Game
{
	EC,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	GeneralFOV,
	HipfireFOV1,
	HipfireFOV2,
	HipfireFOV3,
	HipfireFOV4,
	ZoomFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::EC, {"EuroCops", "EuroCops.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "ZoomFactor", fZoomFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fZoomFactor);

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

	dllModule2 = Memory::GetHandle("Engine.dll");

	dllModule3 = Memory::GetHandle("EntitiesMP.dll");

	return true;
}

static SafetyHookMid GeneralFOVInstructionHook{};
static SafetyHookMid ZoomFOVInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::EC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "8B 87 94 01 00 00 89 86 94 01 00 00 8B 8F 98 01 00 00 89 8E 98 01 00 00 8B 97 9C 01 00 00 89 96 9C 01 00 00 8B 87 A0 01 00 00 89 86 A0 01 00 00 8B 8F A4 01 00 00 89 8E A4 01 00 00 8B 97 A8 01 00 00 89 96 A8 01 00 00 81 C7 AC 01 00 00 8B 0F 8D 86 AC 01 00 00 89 08 8B 57 04 89 50 04 8B 4F 08 89 48 08 8B 57 0C 89 50 0C 5F 8B C6 5E C2 04 00 CC 56",
			dllModule3, "68 ?? ?? ?? ?? 8B CD E8 ?? ?? ?? ?? 83 FE", "68 ?? ?? ?? ?? 8B CD E8 ?? ?? ?? ?? EB", "C7 46 ?? ?? ?? ?? ?? 5E 83 C4 ?? C3 8B 07", "C7 46 ?? ?? ?? ?? ?? 1B C0", "D9 81 ?? ?? ?? ?? E8");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is Engine.dll+{:x}", CameraFOVInstructionsScansResult[GeneralFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Hipfire FOV Instruction 1: Address is EntitiesMP.dll+{:x}", CameraFOVInstructionsScansResult[HipfireFOV1] - (std::uint8_t*)dllModule3);

			spdlog::info("Hipfire FOV Instruction 2: Address is EntitiesMP.dll+{:x}", CameraFOVInstructionsScansResult[HipfireFOV2] - (std::uint8_t*)dllModule3);

			spdlog::info("Hipfire FOV Instruction 3: Address is EntitiesMP.dll+{:x}", CameraFOVInstructionsScansResult[HipfireFOV3] - (std::uint8_t*)dllModule3);

			spdlog::info("Hipfire FOV Instruction 4: Address is EntitiesMP.dll+{:x}", CameraFOVInstructionsScansResult[HipfireFOV4] - (std::uint8_t*)dllModule3);

			spdlog::info("Zoom FOV Instruction: Address is EntitiesMP.dll+{:x}", CameraFOVInstructionsScansResult[ZoomFOV] - (std::uint8_t*)dllModule3);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GeneralFOV], 6);

			GeneralFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GeneralFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = *reinterpret_cast<float*>(ctx.edi + 0x194);

				fNewGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, fAspectRatioScale);

				ctx.eax = std::bit_cast<uintptr_t>(fNewGeneralFOV);
			});

			fNewHipfireFOV = 78.0f * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[HipfireFOV1] + 1, fNewHipfireFOV);

			Memory::Write(CameraFOVInstructionsScansResult[HipfireFOV2] + 1, fNewHipfireFOV);

			Memory::Write(CameraFOVInstructionsScansResult[HipfireFOV3] + 3, fNewHipfireFOV);

			Memory::Write(CameraFOVInstructionsScansResult[HipfireFOV4] + 3, fNewHipfireFOV);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[ZoomFOV], 6);

			ZoomFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ZoomFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = *reinterpret_cast<float*>(ctx.ecx + 0xEC);

				fNewZoomFOV = fCurrentZoomFOV / fZoomFactor;

				FPU::FLD(fNewZoomFOV);
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