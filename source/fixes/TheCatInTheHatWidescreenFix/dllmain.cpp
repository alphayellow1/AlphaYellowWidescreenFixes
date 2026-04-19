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
HMODULE dllModule3;
std::string dllModule2Name;
std::string dllModule3Name;

// Fix details
std::string sFixName = "TheCatInTheHatWidescreenFix";
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
uint32_t iNewWidth;
uint32_t iNewHeight;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;
constexpr float fOriginalCameraFOV1 = 45.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewCameraFOV3;
float fNewCameraFOV4;

// Game detection
enum class Game
{
	CITH,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AR1,
	AR2,
	AR3,
	AR4
};

enum CameraFOVInstructionsScansIndices
{
	FOV1,
	FOV2,
	FOV3,
	FOV4
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CITH, {"The Cat in the Hat", "Cat.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "Width", iNewWidth);
	inipp::get_value(ini.sections["Settings"], "Height", iNewHeight);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iNewWidth);
	spdlog_confparse(iNewHeight);
	spdlog_confparse(fFOVFactor);

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

static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};

void WidescreenFix()
{
	if (bFixActive == true && eGameType == Game::CITH)
	{
		dllModule2 = Memory::GetHandle("Game.dll");
		dllModule3 = Memory::GetHandle("GameEngine.dll");

		dllModule2Name = Memory::GetModuleName(dllModule2);
		dllModule3Name = Memory::GetModuleName(dllModule3);

		fNewAspectRatio = static_cast<float>(iNewWidth) / static_cast<float>(iNewHeight);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(dllModule2, "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? EB");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", dllModule2Name.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScanResult + 4, iNewWidth);

			Memory::Write(ResolutionInstructionsScanResult + 12, iNewHeight);

			Memory::Write(ResolutionInstructionsScanResult + 22, iNewWidth);

			Memory::Write(ResolutionInstructionsScanResult + 30, iNewHeight);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(dllModule2, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 4E",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 4C 24", "8B 80 ?? ?? ?? ?? 8B 4E", dllModule3, "8B 81 ?? ?? ?? ?? 52");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", dllModule2Name.c_str(), AspectRatioInstructionsScansResult[AR1] - (std::uint8_t*)dllModule2);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", dllModule2Name.c_str(), AspectRatioInstructionsScansResult[AR2] - (std::uint8_t*)dllModule2);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", dllModule2Name.c_str(), AspectRatioInstructionsScansResult[AR3] - (std::uint8_t*)dllModule2);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", dllModule3Name.c_str(), AspectRatioInstructionsScansResult[AR4] - (std::uint8_t*)dllModule3);

			Memory::Write(AspectRatioInstructionsScansResult[AR1] + 1, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScansResult[AR2] + 1, fNewAspectRatio);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR3], 6);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR3], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR4], 6);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR4], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 4E ?? 8B 01", "8B 40 ?? 51 52",
		"8b 90 ?? ?? ?? ?? 52", dllModule3, "8B 81 ?? ?? ?? ?? 8B 4C 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", dllModule2Name.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", dllModule2Name.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", dllModule2Name.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", dllModule3Name.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)dllModule3);

			fNewCameraFOV1 = fOriginalCameraFOV1 * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[FOV1] + 1, fNewCameraFOV1);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 3);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.eax + 0x10);

				fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV2);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3], 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = Memory::ReadMem(ctx.eax + 0x98);

				fNewCameraFOV3 = fCurrentCameraFOV3 * fFOVFactor;

				ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV3);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV4], 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV4 = Memory::ReadMem(ctx.ecx + 0x98);

				fNewCameraFOV4 = fCurrentCameraFOV4 * fFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV4);
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