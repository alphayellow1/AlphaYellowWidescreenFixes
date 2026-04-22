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
std::string sFixName = "USRacerWidescreenFix";
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
constexpr float fOriginalGameplayHFOV1 = -0.5f;
constexpr float fOriginalGameplayHFOV2 = 0.5f;
constexpr float fOriginalGameplayVFOV1 = -0.375f;
constexpr float fOriginalGameplayVFOV2 = 0.375f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewGameplayHFOV1;
float fNewGameplayHFOV2;
float fNewGameplayVFOV1;
float fNewGameplayVFOV2;

// Game detection
enum class Game
{
	USR,
	Unknown
};

enum ResolutionListIndices
{
	ResListWidth,
	ResListHeight
};

enum CameraHFOVInstructionsIndices
{
	HFOV1,
	HFOV2,
	HFOV3,
	HFOV4,
	HFOV5,
	HFOV6
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::USR, {"US Racer", "USARacer.exe"}},
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

static SafetyHookMid CameraHFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraHFOVInstruction3Hook{};
static SafetyHookMid CameraHFOVInstruction4Hook{};
static SafetyHookMid CameraHFOVInstruction5Hook{};
static SafetyHookMid CameraHFOVInstruction6Hook{};

enum InstructionType
{
	FLD,
	FSUB,
	FADD,
	EAX
};

void CameraFOVInstructionsMidHook(uintptr_t CameraFOVAddress, InstructionType DestinationInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = Memory::ReadMem(CameraFOVAddress);

	fNewCameraFOV = fCurrentCameraFOV * fAspectRatioScale;

	switch (DestinationInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewCameraFOV);
			break;
		}

		case FSUB:
		{
			FPU::FSUB(fNewCameraFOV);
			break;
		}

		case FADD:
		{
			FPU::FADD(fNewCameraFOV);
			break;
		}

		case EAX:
		{
			ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}
	}
}

void WidescreenFix()
{
	if ((eGameType == Game::USR) && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule,
		"B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? E8 ?? ?? ?? ?? 8D 4C 24",
		"B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution Width List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResListWidth] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Height List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResListHeight] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 1, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 8, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 15, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 22, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 29, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 36, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 43, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 50, iCurrentResX);
			Memory::Write(ResolutionInstructionsScansResult[ResListWidth] + 57, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 1, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 8, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 15, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 22, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 29, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 36, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 43, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 50, iCurrentResY);
			Memory::Write(ResolutionInstructionsScansResult[ResListHeight] + 57, iCurrentResY);
		}

		std::vector<std::uint8_t*> CameraHFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 43 ?? D8 23", "D9 43 ?? D8 03", 
		"D9 81 ?? ?? ?? ?? D8 A1 ?? ?? ?? ?? D8 3D ?? ?? ?? ?? D9 81", "D9 81 ?? ?? ?? ?? D8 81 ?? ?? ?? ?? 89 44 24", "D9 86 ?? ?? ?? ?? D9 C0 D8 4B",
		"8B 86 ?? ?? ?? ?? 57 89 44 24 ?? 8B 51 ?? 8D 4C 24 ?? 89 54 24 ?? 51 8B D0 53");
		if (Memory::AreAllSignaturesValid(CameraHFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[HFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[HFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[HFOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[HFOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[HFOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionsScansResult[HFOV6] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV1], 5);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x4, FLD, ctx);

				CameraFOVInstructionsMidHook(ctx.ebx, FSUB, ctx);
			});

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV2], 5);

			CameraHFOVInstruction2Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebx + 0x4, FLD, ctx);

				CameraFOVInstructionsMidHook(ctx.ebx, FADD, ctx);
			});

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV3], 12);

			CameraHFOVInstruction3Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV4], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x150, FLD, ctx);

				CameraFOVInstructionsMidHook(ctx.ecx + 0x14C, FSUB, ctx);
			});

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV4], 12);

			CameraHFOVInstruction4Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV4], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ecx + 0x14C, FLD, ctx);

				CameraFOVInstructionsMidHook(ctx.ecx + 0x150, FADD, ctx);
			});
			
			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV5], 6);

			CameraHFOVInstruction5Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV5], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x14C, FLD, ctx);
			});

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV6], 6);

			CameraHFOVInstruction6Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV6], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.esi + 0x150, EAX, ctx);
			});
		}

		std::uint8_t* GameplayFOVInstructionsScanResult = Memory::PatternScan(exeModule, "C7 81 ?? ?? ?? ?? ?? ?? ?? ?? D9 99 ?? ?? ?? ?? 85 C9");
		if (GameplayFOVInstructionsScanResult)
		{
			spdlog::info("Gameplay FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), GameplayFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			fNewGameplayHFOV1 = fOriginalGameplayHFOV1 * fFOVFactor;

			fNewGameplayHFOV2 = fOriginalGameplayHFOV2 * fFOVFactor;

			fNewGameplayVFOV1 = fOriginalGameplayVFOV1 * fFOVFactor;

			fNewGameplayVFOV2 = fOriginalGameplayVFOV2 * fFOVFactor;

			Memory::Write(GameplayFOVInstructionsScanResult + 6, fNewGameplayHFOV1);

			Memory::Write(GameplayFOVInstructionsScanResult + 24, fNewGameplayHFOV2);

			Memory::Write(GameplayFOVInstructionsScanResult + 34, fNewGameplayVFOV1);

			Memory::Write(GameplayFOVInstructionsScanResult + 44, fNewGameplayVFOV2);
		}
		else
		{
			spdlog::error("Failed to locate gameplay FOV instructions scan memory address.");
			return;
		}
	}
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		thisModule = hModule;

		Logging();

		Configuration();

		if (DetectGame())
		{
			WidescreenFix();
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