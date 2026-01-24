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
std::string sFixName = "CorvetteEvolutionGTFOVFix";
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
float fNewCameraAspectRatio;
float fNewHUDAspectRatio;
float fNewCameraFOV;
uintptr_t CameraFOV3Address;

// Game detection
enum class Game
{
	CEGT,
	Unknown
};

enum AspectRatioInstructionsIndices
{
	CameraARScan,
	HUDARScan
};

enum CameraFOVInstructionsIndices
{
	FOV1Scan,
	FOV2Scan,
	FOV3Scan,
	FOV4Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CEGT, {"Corvette Evolution GT", "EGT.exe"}},
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

static SafetyHookMid CameraAspectRatioInstructionHook{};
static SafetyHookMid HUDAspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};

enum InstructionType
{
	FLD,
	FMUL,
	EDX
};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float fovFactor, bool isFOVCalculated, InstructionType instructionType, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	if (isFOVCalculated == true)
	{
		fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fovFactor;
	}
	else
	{
		fNewCameraFOV = fCurrentCameraFOV * fovFactor;
	}

	switch (instructionType)
	{
	case FLD:
		FPU::FLD(fNewCameraFOV);
		break;

	case FMUL:
		FPU::FMUL(fNewCameraFOV);
		break;

	case EDX:
		ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
		break;
	}
}

void FOVFix()
{
	if (eGameType == Game::CEGT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> AspectRatioInstructionsScanResult = Memory::PatternScan(exeModule, "D8 76 ?? D9 5C 24 ?? D9 44 24", "8B 54 24 ?? D8 74 24 ?? 89 54 24");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScanResult) == true)
		{
			spdlog::info("Camera Aspect Ratio Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult[CameraARScan] - (std::uint8_t*)exeModule);

			spdlog::info("HUD Aspect Ratio Instruction Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScanResult[HUDARScan] - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(AspectRatioInstructionsScanResult[CameraARScan], 3);
			
			CameraAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScanResult[CameraARScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraAspectRatio = *reinterpret_cast<float*>(ctx.esi + 0x20);

				fNewCameraAspectRatio = fCurrentCameraAspectRatio * fAspectRatioScale;

				FPU::FDIV(fNewCameraAspectRatio);
			});

			Memory::WriteNOPs(AspectRatioInstructionsScanResult[HUDARScan], 4);

			HUDAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScanResult[HUDARScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDAspectRatio = *reinterpret_cast<float*>(ctx.esp + 0x3C);

				fNewHUDAspectRatio = fCurrentHUDAspectRatio * fAspectRatioScale;

				ctx.edx = std::bit_cast<uintptr_t>(fNewHUDAspectRatio);
			});
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "D9 84 30 ?? ?? ?? ?? D8 0D", "D9 44 24 ?? 8B 44 24 ?? D8 0D ?? ?? ?? ?? 56", "8B 15 ?? ?? ?? ?? D8 44 24", "D8 0D ?? ?? ?? ?? D9 46 ?? D8 4E");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScanResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScanResult[FOV4Scan] - (std::uint8_t*)exeModule);
			
			Memory::WriteNOPs(CameraFOVInstructionsScanResult[FOV1Scan], 7);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScanResult[FOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax + ctx.esi + 0x1F8, 1.0f, true, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult[FOV2Scan], 4);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScanResult[FOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esp + 0x8, fFOVFactor, false, FLD, ctx);
			});			

			CameraFOV3Address = reinterpret_cast<uintptr_t>(Memory::GetPointerFromAddress<uint32_t>(CameraFOVInstructionsScanResult[FOV3Scan] + 2, Memory::PointerMode::Absolute));

			Memory::WriteNOPs(CameraFOVInstructionsScanResult[FOV3Scan], 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScanResult[FOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV3Address, 1.0f, true, EDX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult[FOV4Scan], 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScanResult[FOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(CameraFOV3Address, 1.0f, true, FMUL, ctx);
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