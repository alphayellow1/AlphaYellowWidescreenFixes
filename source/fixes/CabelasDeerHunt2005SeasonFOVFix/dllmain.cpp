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

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule2 = nullptr;
HMODULE dllModule3 = nullptr;

// Fix details
std::string sFixName = "CabelasDeerHunt2005SeasonFOVFix";
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
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;



// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
uint32_t iInsideCar;
uint8_t* InsideCarValueAddress;

// Game detection
enum class Game
{
	CDH2005S,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CDH2005S, {"Cabela's Deer Hunt 2005 Season", "udh2005.exe"}},
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

	while ((dllModule2 = GetModuleHandleA("EngineDll6.dll")) == nullptr)
	{
		spdlog::warn("EngineDll6.dll not loaded yet. Waiting...");
		Sleep(100);
	}

	spdlog::info("Successfully obtained handle for EngineDll6.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

static SafetyHookMid InsideCarTriggerInstructionHook{};
static SafetyHookMid CameraFOVInstructionHook{};
static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};

void FOVFix()
{
	if (eGameType == Game::CDH2005S && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* InsideCarTriggerInstructionScanResult = Memory::PatternScan(exeModule, "A1 ?? ?? ?? ?? 83 C4 04 3B C3 75 1C 8B 86 68 1C 00 00");
		if (InsideCarTriggerInstructionScanResult)
		{
			spdlog::info("Inside Car Trigger Instruction: Address is {:s}+{:x}", sExeName.c_str(), InsideCarTriggerInstructionScanResult - (std::uint8_t*)exeModule);

			InsideCarValueAddress = Memory::GetPointerFromAddress<uint32_t>(InsideCarTriggerInstructionScanResult + 1, Memory::PointerMode::Absolute);			

			InsideCarTriggerInstructionHook = safetyhook::create_mid(InsideCarTriggerInstructionScanResult, [](SafetyHookContext& ctx)
			{
				iInsideCar = *reinterpret_cast<uint32_t*>(InsideCarValueAddress);
			});
		}
		else
		{
			spdlog::error("Failed to locate inside car trigger instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 83 D0 00 00 00 D8 0D ?? ?? ?? ??");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is EngineDll6.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.ebx + 0xD0);

				if (iInsideCar == 0) // Outside car
				{
					if (fCurrentCameraFOV == 75.0f)
					{
						fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
					}
					else
					{
						fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale);
					}
				}
				else if (iInsideCar == 1) // Inside car
				{
					fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, fAspectRatioScale);
				}

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionsScanResult = Memory::PatternScan(dllModule2, "D9 93 DC 00 00 00 D9 83 D4 00 00 00");
		if (AspectRatioInstructionsScanResult)
		{
			spdlog::info("Aspect Ratio Instructions Scan: Address is EngineDll6.dll+{:x}", AspectRatioInstructionsScanResult - (std::uint8_t*)dllModule2);			

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScanResult + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.ebx + 0xD4) = 0.75f / (fNewAspectRatio / fOldAspectRatio);
			});

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScanResult + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.ebx + 0xD8) = 1.0f;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructions memory address.");
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