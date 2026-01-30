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
std::string sFixName = "TowerOfTheAncientsFOVFix";
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

// Game detection
enum class Game
{
	TOTA,
	Unknown
};

enum CameraFOVInstructionsIndex
{
	HFOV1Scan,
	HFOV2Scan,
	HFOV3Scan,
	HFOV4Scan,
	HFOV5Scan,
	VFOV1Scan,
	VFOV2Scan,
	VFOV3Scan,
	VFOV4Scan,
	VFOV5Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TOTA, {"Tower of the Ancients", "TOWER.EXE"}},
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
static SafetyHookMid CameraHFOVInstruction3Hook{};
static SafetyHookMid CameraHFOVInstruction4Hook{};
static SafetyHookMid CameraHFOVInstruction5Hook{};
static SafetyHookMid CameraVFOVInstruction1Hook{};
static SafetyHookMid CameraVFOVInstruction2Hook{};
static SafetyHookMid CameraVFOVInstruction3Hook{};
static SafetyHookMid CameraVFOVInstruction4Hook{};
static SafetyHookMid CameraVFOVInstruction5Hook{};

enum DestinationInstruction
{
	FLD,
	FMUL,
	ECX,
	EDX,
};

void CameraFOVInstructionsMidHook(uintptr_t SourceAddress, float fARScale, float fovFactor, DestinationInstruction destinationInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(SourceAddress);

	fNewCameraFOV = fCurrentCameraFOV * fARScale * fovFactor;

	switch (destinationInstruction)
	{
	case FLD:
		FPU::FLD(fNewCameraFOV);
		break;

	case FMUL:
		FPU::FMUL(fNewCameraFOV);
		break;

	case ECX:
		ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV);
		break;

	case EDX:
		ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
		break;
	}
}

void FOVFix()
{
	if (eGameType == Game::TOTA && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 46 ?? 8B 46 ?? 8D 4E ?? D8 0D ?? ?? ?? ?? 83 C0 ?? D9 C0 D8 08 D9 5C 24 ?? D9 40 ?? D8 C9 D9 5C 24 ?? D9 40 ?? D8 C9 D9 5C 24 ?? D8 4E ?? D8 2D", "8B 4E ?? 89 0D ?? ?? ?? ?? 8B 56 ?? 89 15 ?? ?? ?? ?? E8", "D9 41 ?? 83 C0", "D8 49 ?? 8D B1", "D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 49",
		"D9 46 ?? D8 0D ?? ?? ?? ?? D9 40 ?? D8 C9 D9 5C 24 ?? D9 40 ?? D8 C9 D9 5C 24 ?? D9 40 ?? D8 C9 D9 5C 24 ?? D8 4E ?? D8 05", "8B 56 ?? 89 15 ?? ?? ?? ?? E8", "D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 C1", "D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 C1", "D8 49 ?? D9 5C 24 ?? D9 40 ?? D8 C1");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV3Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV3Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Camera VFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOV5Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV1Scan], 3);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x68, 1.0f / fAspectRatioScale, 1.0f / fFOVFactor, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV2Scan], 3);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x68, 1.0f / fAspectRatioScale, 1.0f / fFOVFactor, ECX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV3Scan], 3);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x60, fAspectRatioScale, fFOVFactor, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV4Scan], 3);

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x60, fAspectRatioScale, fFOVFactor, FMUL, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOV5Scan], 3);

			CameraHFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOV5Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x60, fAspectRatioScale, fFOVFactor, FMUL, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV1Scan], 3);

			CameraVFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV1Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x6C, 1.0f, 1.0f / fFOVFactor, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV2Scan], 3);

			CameraVFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV2Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x6C, 1.0f, 1.0f / fFOVFactor, EDX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV3Scan], 3);

			CameraVFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV3Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x64, 1.0f, fFOVFactor, FMUL, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV4Scan], 3);

			CameraVFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV4Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x64, 1.0f, fFOVFactor, FMUL, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOV5Scan], 3);

			CameraVFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOV5Scan], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x64, 1.0f, fFOVFactor, FMUL, ctx);
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