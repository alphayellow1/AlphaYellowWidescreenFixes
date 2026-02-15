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
std::string sFixName = "PrisonerOfWarWidescreenFix";
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
constexpr float fNewAspectRatio2 = 0.75f;
constexpr float fNewFrustumCullingFOV = 3.1f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewGameplayFOV;
float fNewCameraHFOV;

// Game detection
enum class Game
{
	POWGAME_EN,
	POWGAME_RU,
	POWCONFIG_EN,
	POWCONFIG_RU,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	AR1,
	Codecave,
	AR2,
	HFOV
};

enum CameraFOVInstructionsIndices
{
	GameplayFOV,
	FrustumCullingFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::POWGAME_EN, {"Prisoner of War (English)", "Colditz.exe"}},
	{Game::POWGAME_RU, {"Prisoner of War (Russian)", "RussianPOW.exe"}},
	{Game::POWCONFIG_EN, {"Prisoner of War - Launcher (English)", "PrisonerLaunchEnglish.exe"}},
	{Game::POWCONFIG_RU, {"Prisoner of War - Launcher (Russian)", "PrisonerLaunchRussian.exe"}},
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

static SafetyHookMid GameplayCameraFOVInstructionHook{};
static SafetyHookMid FrustumCullingFOVInstructionHook{};
static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid CameraHFOVInstructionHook{};

void FOVFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::POWCONFIG_EN || eGameType == Game::POWCONFIG_RU)
		{
			std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 44 24 ?? ?? ?? ?? ?? EB ?? C7 44 24 ?? ?? ?? ?? ?? EB ?? C7 44 24 ?? ?? ?? ?? ?? EB");
			if (ResolutionListScanResult)
			{
				spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

				static std::string sNewResString = "-res:" + std::to_string(iCurrentResX) + "," + std::to_string(iCurrentResY);

				const char* cNewResolutionListString = sNewResString.c_str();

				Memory::Write(ResolutionListScanResult + 4, cNewResolutionListString);

				Memory::Write(ResolutionListScanResult + 14, cNewResolutionListString);

				Memory::Write(ResolutionListScanResult + 24, cNewResolutionListString);

				Memory::Write(ResolutionListScanResult + 34, cNewResolutionListString);
			}
			else
			{
				spdlog::error("Failed to locate resolution list scan memory address.");
				return;
			}
		}

		if (eGameType == Game::POWGAME_EN || eGameType == Game::POWGAME_RU)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "DB 05 ?? ?? ?? ?? 8B C1", "7A 2E 65 78 65", "E8 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 4E", "D8 3D ?? ?? ?? ?? D9 18");
			if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
			{
			    spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AR1] - (std::uint8_t*)exeModule);

			    spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HFOV] - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR1], 6);

				AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR1], [](SafetyHookContext& ctx)
				{
					FPU::FLD(fNewAspectRatio2);
				});

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[AR1] + 10, 6);

				if (eGameType == Game::POWGAME_EN)
				{
					Memory::PatchBytes(AspectRatioInstructionsScansResult[Codecave] + 6, "\xDB\x05\xC8\x70\x66\x00\x8B\xC1\x33\xC9\xDA\x35\xC4\x70\x66\x00\xC7\x40\x2C\x00\x00\x80\x3F\x89\x48\x3C\x89\x48\x34\x89\x48\x30\x89\x48\x24\x89\x48\x20\x89\x48\x1C\x89\x48\x18\x89\x48\x10\x89\x48\x0C\x89\x48\x08\x89\x48\x04\xD8\x4C\x24\x0C\xD8\x0D\xCC\xC2\x60\x00\xD9\xF2\xDD\xD8\xD8\x3D\x44\xC4\x60\x00\xD9\x44\x24\x08\xD8\x64\x24\x04\xD8\x7C\x24\x08\xD9\x44\x24\x0C\xD8\x0D\xCC\xC2\x60\x00\xD9\xF2\xDD\xD8\xD8\x3D\x44\xC4\x60\x00\xD9\x18\xD9\xC9\xD9\x58\x14\xD9\x50\x28\xD8\x4C\x24\x04\xD9\xE0\xD9\x58\x38\xC2\x0C\x00");
				}
				else if (eGameType == Game::POWGAME_RU)
				{
					Memory::PatchBytes(AspectRatioInstructionsScansResult[Codecave] + 6, "\xDB\x05\x00\xDA\x66\x00\x8B\xC1\x33\xC9\xDA\x35\xFC\xD9\x66\x00\xC7\x40\x2C\x00\x00\x80\x3F\x89\x48\x3C\x89\x48\x34\x89\x48\x30\x89\x48\x24\x89\x48\x20\x89\x48\x1C\x89\x48\x18\x89\x48\x10\x89\x48\x0C\x89\x48\x08\x89\x48\x04\xD8\x4C\x24\x0C\xD8\x0D\xCC\xD2\x60\x00\xD9\xF2\xDD\xD8\xD8\x3D\x44\xD4\x60\x00\xD9\x44\x24\x08\xD8\x64\x24\x04\xD8\x7C\x24\x08\xD9\x44\x24\x0C\xD8\x0D\xCC\xD2\x60\x00\xD9\xF2\xDD\xD8\xD8\x3D\x44\xD4\x60\x00\xD9\x18\xD9\xC9\xD9\x58\x14\xD9\x50\x28\xD8\x4C\x24\x04\xD9\xE0\xD9\x58\x38\xC2\x0C\x00");
				}

				Memory::PatchCALL(AspectRatioInstructionsScansResult[AR2], AspectRatioInstructionsScansResult[Codecave] + 6, Memory::CallType::Relative);

				Memory::WriteNOPs(AspectRatioInstructionsScansResult[HFOV], 6);

				fNewCameraHFOV = 1.0f / fAspectRatioScale;

				CameraHFOVInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HFOV], [](SafetyHookContext& ctx)
				{
					FPU::FDIVR(fNewCameraHFOV);
				});
			}

			std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 06 D8 0D ?? ?? ?? ?? 51", "D9 81 ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 8B 44 24 ?? 8B 54 24");
			if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
			{
				spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[GameplayFOV] - (std::uint8_t*)exeModule);

				spdlog::info("Frustum Culling Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FrustumCullingFOV] - (std::uint8_t*)exeModule);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[GameplayFOV], 2);

				GameplayCameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV], [](SafetyHookContext& ctx)
				{
					float& fCurrentGameplayFOV = *reinterpret_cast<float*>(ctx.esi);

					fNewGameplayFOV = fCurrentGameplayFOV * fFOVFactor;
					
					FPU::FLD(fNewGameplayFOV);
				});

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FrustumCullingFOV], 6);

				FrustumCullingFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FrustumCullingFOV], [](SafetyHookContext& ctx)
				{
					FPU::FLD(fNewFrustumCullingFOV);
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