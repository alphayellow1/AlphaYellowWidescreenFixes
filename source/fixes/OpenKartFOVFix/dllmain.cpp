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
std::string sFixName = "OpenKartFOVFix";
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
int iCurrentResX;
int iCurrentResY;
bool bFixActive;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	OPENKART_FULL,
	OPENKART_DEMO,
	Unknown
};

enum CameraHFOVInstructionsIndex
{
	CameraHFOV1Scan,
	CameraHFOV2Scan
};

enum CameraVFOVInstructionsIndex
{
	CameraVFOV1Scan,
	CameraVFOV2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::OPENKART_FULL, {"Open Kart (Full Version)", "Kart.exe"}},
	{Game::OPENKART_DEMO, {"Open Kart (Demo)", "KartDemo.exe"}},
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

static SafetyHookMid CameraHFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};

void CameraFOVInstructionsMidHook(uintptr_t CameraFOVAddress, float fARScale, uintptr_t DestinationAddress)
{
	float fCurrentCameraFOV = std::bit_cast<float>(CameraFOVAddress);

	fNewCameraFOV = fCurrentCameraFOV * fARScale * fFOVFactor;

	*reinterpret_cast<float*>(DestinationAddress) = fNewCameraFOV;
}

void FOVFix()
{
	if ((eGameType == Game::OPENKART_FULL || eGameType == Game::OPENKART_DEMO) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraHFOVInstructionsScansResult = Memory::PatternScan(exeModule, "89 91 ?? ?? ?? ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 8B 84 24", "89 81 ?? ?? ?? ?? 8B 84 24");

		std::vector<std::uint8_t*> CameraVFOVInstructionsScansResult = Memory::PatternScan(exeModule, "89 91 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? D8 35", "89 81 ?? ?? ?? ?? D8 35");

		if (Memory::AreAllSignaturesValid(CameraHFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[CameraHFOV2Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.edx, fAspectRatioScale, ctx.ecx + 0x144);
			});

			Memory::PatchBytes(CameraHFOVInstructionsScansResult[CameraHFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[CameraHFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax, fAspectRatioScale, ctx.ecx + 0x148);
			});
		}

		if (Memory::AreAllSignaturesValid(CameraVFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionsScansResult[CameraVFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionsScansResult[CameraVFOV2Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraVFOVInstructionsScansResult[CameraVFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraVFOVInstructionsScansResult[CameraVFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.edx, 1.0f, ctx.ecx + 0x14C);
			});

			Memory::PatchBytes(CameraVFOVInstructionsScansResult[CameraVFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraVFOVInstructionsScansResult[CameraVFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax, 1.0f, ctx.ecx + 0x150);
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