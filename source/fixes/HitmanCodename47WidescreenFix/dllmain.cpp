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
std::string sFixName = "HitmanCodename47WidescreenFix";
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
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;
float fDrawDistanceFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
double dNewGeneralFOV;
double dNewCutscenesFOV;
float fNewHipfireFOV;
float fNewDrawDistance;

// Game detection
enum class Game
{
	HC47,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	GeneralFOV,
	CutscenesFOV,
	HipfireFOV
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::HC47, {"Hitman: Codename 47", "Hitman.Exe"}},
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
	inipp::get_value(ini.sections["Settings"], "DrawDistanceFactor", fDrawDistanceFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fDrawDistanceFactor);

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

	dllModule3 = Memory::GetHandle({ "RenderD3D.dll", "RenderOpenGL.dll" });

	return true;
}

static SafetyHookMid ResolutionInstructionsHook{};
static SafetyHookMid GeneralFOVInstructionHook{};
static SafetyHookMid CutscenesFOVInstructionHook{};
static SafetyHookMid DrawDistanceInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::HC47 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(dllModule3, "0F 84 ?? ?? ?? ?? 8B 42");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is RenderD3D.dll+{:x}", ResolutionInstructionsScanResult - (std::uint8_t*)dllModule3);

			Memory::WriteNOPs(ResolutionInstructionsScanResult, 6);

			Memory::WriteNOPs(ResolutionInstructionsScanResult + 9, 7);

			Memory::Write(ResolutionInstructionsScanResult + 17, iCurrentResX);

			Memory::Write(ResolutionInstructionsScanResult + 22, iCurrentResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		dllModule2 = Memory::GetHandle("HitmanDlc.dlc", 50);

		std::vector<std::uint8_t*> CameraFOVInstructionsScanResult = Memory::PatternScan(dllModule2, "DD 85 ?? ?? ?? ?? 8B 11", "DD 44 24 ?? DC 0D ?? ?? ?? ?? 6A", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF FF 90 ?? ?? ?? ?? 5F 89 5E");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScanResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is HitmanDlc.dlc+{:x}", CameraFOVInstructionsScanResult[GeneralFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Cutscenes FOV Instruction: Address is HitmanDlc.dlc+{:x}", CameraFOVInstructionsScanResult[CutscenesFOV] - (std::uint8_t*)dllModule2);

			spdlog::info("Hipfire FOV Instruction: Address is HitmanDlc.dlc+{:x}", CameraFOVInstructionsScanResult[HipfireFOV] - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(CameraFOVInstructionsScanResult[GeneralFOV], 6); // NOP out the original instruction

			GeneralFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[GeneralFOV], [](SafetyHookContext& ctx)
			{
				double& dCurrentGeneralFOV = *reinterpret_cast<double*>(ctx.ebp + 0x16E);

				if (dCurrentGeneralFOV != dNewGeneralFOV)
				{
					if (dCurrentGeneralFOV < 0.0)
					{
						dNewGeneralFOV = Maths::CalculateNewFOV_RadBased(0.7853981853, fAspectRatioScale);
					}
					else
					{
						dNewGeneralFOV = Maths::CalculateNewFOV_RadBased(dCurrentGeneralFOV, fAspectRatioScale);
					}	
				}

				FPU::FLD(dNewGeneralFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult[CutscenesFOV], 4); // NOP out the original instruction

			CutscenesFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				double& dCurrentCutscenesFOV = *reinterpret_cast<double*>(ctx.esp + 0x14);

				dNewCutscenesFOV = Maths::CalculateNewFOV_DegBased(dCurrentCutscenesFOV, fAspectRatioScale);

				FPU::FLD(dNewCutscenesFOV);
			});

			fNewHipfireFOV = 3.324630499f * powf(fFOVFactor, 0.075f);

			Memory::Write(CameraFOVInstructionsScanResult[HipfireFOV] + 1, fNewHipfireFOV);
		}

		std::uint8_t* DrawDistanceInstructionScanResult = Memory::PatternScan(dllModule2, "8B 85 ?? ?? ?? ?? 8B 4B ?? 8B 11 50 8B 85 ?? ?? ?? ?? 50 FF 92 ?? ?? ?? ?? 8B 43");
		if (DrawDistanceInstructionScanResult)
		{
			spdlog::info("Draw Distance Instruction: Address is HitmanDlc.dlc+{:x}", DrawDistanceInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(DrawDistanceInstructionScanResult, 6); // NOP out the original instruction

			DrawDistanceInstructionHook = safetyhook::create_mid(DrawDistanceInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentDrawDistance = *reinterpret_cast<float*>(ctx.ebp + 0x18A);

				fNewDrawDistance = fCurrentDrawDistance * fDrawDistanceFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewDrawDistance);
			});
		}
		else
		{
			spdlog::error("Failed to locate draw distance instruction memory address.");
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