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
std::string sFixName = "StateOfEmergencyWidescreenFix";
std::string sFixVersion = "1.6";
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
float fNewCameraFOV1;
float fNewCameraFOV2;
uint8_t* HorizontalCullingAddress;
float fNewCameraHFOVCulling;

// Game detection
enum class Game
{
	SOE,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan,
	FOV4Scan,
	HFOVCullingScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SOE, {"State of Emergency", "KaosPC.exe"}},
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

static SafetyHookMid ResolutionWidthInstructionHook{};
static SafetyHookMid ResolutionHeightInstructionHook{};
static SafetyHookMid CameraHFOVCullingInstructionHook{};

void FOVFix()
{
	if (eGameType == Game::SOE && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "8B 04 10 57 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6B C0 ?? 8B 0E 8B 54 08 ??");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScanResult, 3);			

			ResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScanResult + 19, 4);			

			ResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 19, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionsScanResult = Memory::PatternScan(exeModule, "C7 44 24 48 39 8E E3 3F 75 08 C7 44 24 48 00 00 A0 3F");
		if (AspectRatioInstructionsScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult + 10 - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioInstructionsScanResult + 4, fNewAspectRatio);

			Memory::Write(AspectRatioInstructionsScanResult + 14, fNewAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructions scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "68 00 80 4A 44 68 00 00 80 47 68 00 00 80 3F 6A 00 E8 F8 8E 04 00", "68 00 80 4A 44 68 00 00 80 47 33 ED 68 00 00 80 3F", "68 00 80 4A 44 68 00 00 80 47 68 00 00 80 3F 6A 00 E8 9F 39 01 00 6A 01", "68 00 00 61 44 68 00 00 80 47 68 00 00 80 3F 53 C7 05 40 6E 4E 00 00 00 00 00 C7 05 3C 6E 4E 00 00 00 00 00", "D9 05 ?? ?? ?? ?? D8 4C 24 50 C7 05 F4 E4 4D 00 00 00 00 00 C7 05 F0 E4 4D 00 00 00 00 00 C7 05 DC E4 4D 00 00 00 00 00");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult))
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Culling Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOVCullingScan] - (std::uint8_t*)exeModule);

			fNewCameraFOV1 = 810.0f / fFOVFactor;

			fNewCameraFOV2 = 900.0f / fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[FOV1Scan] + 1, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[FOV2Scan] + 1, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[FOV3Scan] + 1, fNewCameraFOV1);

			Memory::Write(CameraFOVInstructionsScansResult[FOV4Scan] + 1, fNewCameraFOV2);

			HorizontalCullingAddress = Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScansResult[HFOVCullingScan] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOVCullingScan], 6);

			CameraHFOVCullingInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOVCullingScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOVCulling = *reinterpret_cast<float*>(HorizontalCullingAddress);

				fNewCameraHFOVCulling = fCurrentCameraHFOVCulling / fAspectRatioScale;

				FPU::FLD(fNewCameraHFOVCulling);
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
