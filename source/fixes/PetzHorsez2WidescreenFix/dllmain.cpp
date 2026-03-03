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
#include <algorithm>
#include <cmath>
#include <bit>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "PetzHorsez2WidescreenFix";
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
constexpr float fOriginalHUDFOV = 1.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewHUDFOV;
float fNewCameraFOV;

// Game detection
enum class Game
{
	PH2_CONFIG,
	PH2_GAME,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResList1,
	ResList2,
	ResList3,
	_16_10_String
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	FOV5,
	FOV6,
	FOV7,
	FOV8,
	FOV9,
	FOV10,
	FOV11,
	FOV12,
	FOV13,
	FOV14,
	FOV15,
	FOV16,
	FOV17,
	FOV18,
	FOV19,
	FOV20
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PH2_CONFIG, {"Petz Horsez 2 - Configuration", "HorsezOption.exe"}},
	{Game::PH2_GAME, {"Petz Horsez 2", "Jade.exe"}},
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

enum destInstruction
{
	FLD,
	FMUL,
	ECX,
	EDX,
	EAX
};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float fovFactor, destInstruction DestInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fovFactor;

	switch (DestInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewCameraFOV);
			break;
		}

		case FMUL:
		{
			FPU::FMUL(fNewCameraFOV);
			break;
		}

		case ECX:
		{
			ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}
	
		case EDX:
		{
			ctx.edx = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}

		case EAX:
		{
			ctx.eax = std::bit_cast<uintptr_t>(fNewCameraFOV);
			break;
		}
	}
}

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};
static SafetyHookMid CameraFOVInstruction7Hook{};
static SafetyHookMid CameraFOVInstruction8Hook{};
static SafetyHookMid CameraFOVInstruction9Hook{};
static SafetyHookMid CameraFOVInstruction10Hook{};
static SafetyHookMid CameraFOVInstruction11Hook{};
static SafetyHookMid CameraFOVInstruction12Hook{};
static SafetyHookMid CameraFOVInstruction13Hook{};
static SafetyHookMid CameraFOVInstruction14Hook{};
static SafetyHookMid CameraFOVInstruction15Hook{};
static SafetyHookMid CameraFOVInstruction16Hook{};
static SafetyHookMid CameraFOVInstruction17Hook{};
static SafetyHookMid CameraFOVInstruction18Hook{};
static SafetyHookMid CameraFOVInstruction19Hook{};

void WidescreenFix()
{
	if (bFixActive == true)
	{
		if (eGameType == Game::PH2_CONFIG)
		{
			std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "81 7D ?? ?? ?? ?? ?? 75 ?? 81 7D ?? ?? ?? ?? ?? 74 ?? 81 7D ?? ?? ?? ?? ?? 75 ?? 81 7D ?? ?? ?? ?? ?? 74 ?? 81 7D ?? ?? ?? ?? ?? 75 ?? 81 7D ?? ?? ?? ?? ?? 75 ?? 8B F4 68 ?? ?? ?? ?? 8D 45 ?? 50 FF 15 ?? ?? ?? ?? 83 C4 ?? 3B F4 E8 ?? ?? ?? ?? 68",
			"81 7D ?? ?? ?? ?? ?? 75 ?? 81 BD ?? ?? ?? ?? ?? ?? ?? ?? 74 ?? 81 7D ?? ?? ?? ?? ?? 75 ?? 81 BD ?? ?? ?? ?? ?? ?? ?? ?? 74 ?? 81 7D ?? ?? ?? ?? ?? 75 ?? 81 BD ?? ?? ?? ?? ?? ?? ?? ?? 75 ?? 8B F4 68 ?? ?? ?? ?? 8D 45 ?? 50 FF 15 ?? ?? ?? ?? 83 C4 ?? 3B F4 E8 ?? ?? ?? ?? 8B 4D ?? E8 ?? ?? ?? ?? 8D 45", "8B 44 11 10 89 45 AC 81 7D C4 00 05 00 00",
			"68 ?? ?? ?? ?? 8B 45 ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 8B 4D ?? E8 ?? ?? ?? ?? 8B 45");
			if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
			{
				spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResList1] - (std::uint8_t*)exeModule);

				spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResList2] - (std::uint8_t*)exeModule);

				spdlog::info("Resolution List 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ResList3] - (std::uint8_t*)exeModule);

				spdlog::info("16:10 String Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[_16_10_String] - (std::uint8_t*)exeModule);

				static std::string sAspectRatio = Maths::GetSimpliedAspectRatio(iCurrentResX, iCurrentResY);

				const char* cSimplifiedAspectRatio = sAspectRatio.c_str();

				Memory::WriteNOPs(ResolutionInstructionsScansResult[ResList1], 36);

				Memory::Write(ResolutionInstructionsScansResult[ResList1] + 39, iCurrentResX);

				Memory::Write(ResolutionInstructionsScansResult[ResList1] + 48, iCurrentResY);

				Memory::Write(ResolutionInstructionsScansResult[ResList1] + 57, cSimplifiedAspectRatio);
				
				Memory::WriteNOPs(ResolutionInstructionsScansResult[ResList2], 42);

				Memory::Write(ResolutionInstructionsScansResult[ResList2] + 45, iCurrentResX);

				Memory::Write(ResolutionInstructionsScansResult[ResList2] + 57, iCurrentResY);

				Memory::Write(ResolutionInstructionsScansResult[ResList2] + 66, cSimplifiedAspectRatio);

				Memory::WriteNOPs(ResolutionInstructionsScansResult[ResList3] + 7, 36);

				Memory::Write(ResolutionInstructionsScansResult[ResList3] + 46, iCurrentResX);

				Memory::Write(ResolutionInstructionsScansResult[ResList3] + 59, iCurrentResY);

				Memory::Write(ResolutionInstructionsScansResult[_16_10_String] + 1, cSimplifiedAspectRatio);
			}
		}
		
		if (eGameType == Game::PH2_GAME)
		{
			fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

			fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

			std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "d9 82 ?? ?? ?? ?? 5d", "d9 80 ?? ?? ?? ?? d8 81 ?? ?? ?? ?? 51 d9 1c 24 6a",
			"8B 91 D8 0E 00 00 89 55 98 E9 FD 02 00 00 8B 85 7C FF FF FF", "8B 88 F4 0E 00 00 89 4D 98 E9 FD 02 00 00 8B 95 7C FF FF FF", "8b 91 ?? ?? ?? ?? 52 8b 45 ?? 8b 88 ?? ?? ?? ?? 51 8b 55 ?? 8b 82 ?? ?? ?? ?? 50 8b 4d ?? 8b 91 ?? ?? ?? ?? 52 8d 45",
			"d8 8c 8a ?? ?? ?? ?? 8b 45 ?? d8 40", "8B 88 48 0F 00 00 89 4D 98 E9 62 02 00 00 8B 95 7C FF FF FF 81 C2 74 0F 00 00 8B 02 89 45 9C 8B 4A 04", "8b 88 ?? ?? ?? ?? 51 8b 55 ?? 8b 82 ?? ?? ?? ?? 50 e8 ?? ?? ?? ?? 83 c4 ?? d9 1c 24",
			"8B 88 38 12 00 00 89 4D 98 E9 E0 00 00 00 8B 95 7C FF FF FF 81 C2 64 12 00 00 8B 02 89 45 9C 8B 4A 04 89 4D A0", "8B 82 10 0F 00 00 89 45 98 E9 B0 02 00 00 8B 8D 7C FF FF FF 81 C1 3C 0F 00 00 8B 11 89 55 9C 8B 41 04 89 45 A0 8B 49 08",
			"8B 82 74 11 00 00 89 45 98 E9 16 02 00 00 8B 8D 7C FF FF FF 81 C1 A0 11 00 00 8B 11 89 55 9C 8B 41 04 89 45 A0 8B 49 08 89 4D A4", "8b 91 ?? ?? ?? ?? 52 6a ?? e8 ?? ?? ?? ?? 83 c4 ?? 8b 45 ?? 05",
			"8B 88 90 11 00 00 89 4D 98 E9 C8 01 00 00 8B 95 7C FF FF FF 81 C2 BC 11 00 00 8B 02 89 45 9C 8B 4A 04 89 4D A0 8B 52 08 89 55 A4", "8B 88 AC 11 00 00 89 4D 98 E9 C8 01 00 00 8B 95 7C FF FF FF 81 C2 D8 11 00 00 8B 02 89 45 9C 8B 4A 04 89 4D A0 8B 52 08 89 55 A4",
			"8B 91 70 12 00 00 89 55 98 E9 93 00 00 00 8B 85 7C FF FF FF 05 9C 12 00 00 8B 08 89 4D 9C 8B 50 04 89 55 A0 8B 40 08", "8b 88 ?? ?? ?? ?? 89 4d ?? e9 ?? ?? ?? ?? 8b 95 ?? ?? ?? ?? 81 c2 ?? ?? ?? ?? 8b 02 89 45 ?? 8b 4a ?? 89 4d ?? 8b 52 ?? 89 55 ?? 8b 85 ?? ?? ?? ?? 05 ?? ?? ?? ?? 8b 08 89 4d ?? 8b 50 ?? 89 55 ?? 8b 40 ?? 89 45 ?? 8b 8d ?? ?? ?? ?? 8b 91 ?? ?? ?? ?? 89 55 ?? eb",
			"8b 82 ?? ?? ?? ?? 89 45 ?? e9 ?? ?? ?? ?? 8b 8d ?? ?? ?? ?? 81 c1 ?? ?? ?? ?? 8b 11 89 55 ?? 8b 41 ?? 89 45 ?? 8b 49 ?? 89 4d ?? 8b 95 ?? ?? ?? ?? 81 c2 ?? ?? ?? ?? 8b 02 89 45 ?? 8b 4a ?? 89 4d ?? 8b 52 ?? 89 55 ?? 8b 85 ?? ?? ?? ?? 8b 88 ?? ?? ?? ?? 89 4d ?? eb",
			"8B 91 80 0F 00 00 89 55 98 E9 15 02 00 00 8B 85 7C FF FF FF 05 AC 0F 00 00 8B 08 89 4D 9C 8B 50 04 89 55 A0 8B 40 08 89 45 A4", "8b 82 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? eb ?? 8b 4d ?? 8b 55 ?? 8b 82 ?? ?? ?? ?? 89 81 ?? ?? ?? ?? 6a", "c7 45 ?? ?? ?? ?? ?? c7 45 ?? ?? ?? ?? ?? c7 45 ?? ?? ?? ?? ?? 6a ?? 6a ?? 8b 4d");
			if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
			{
				Memory::WriteNOPs(CameraFOVInstructionsScansResult, FOV1, FOV5, 0, 6);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV6], 7);

				Memory::WriteNOPs(CameraFOVInstructionsScansResult, FOV7, FOV19, 0, 6);
				
				CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.edx + 0xCF55E0, 1.0f, FLD, ctx);
				});

				CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0xC84, fFOVFactor, FLD, ctx);
				});

				CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.ecx + 0xED8, 1.0f, EDX, ctx);
				});

				CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0xEF4, 1.0f, ECX, ctx);
				});

				CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.ecx + 0x110, 1.0f, EDX, ctx);
				});

				CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.edx + ctx.ecx * 0x4 + 0x258, fFOVFactor, FMUL, ctx);
				});

				CameraFOVInstruction7Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV7], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0xF48, 1.0f, ECX, ctx);
				});

				CameraFOVInstruction8Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV8], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0xE1C, 1.0f, ECX, ctx);
				});

				CameraFOVInstruction9Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV9], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0x1238, 1.0f, ECX, ctx);
				});

				CameraFOVInstruction10Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV10], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.edx + 0xF10, 1.0f, EAX, ctx);
				});

				CameraFOVInstruction11Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV11], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.edx + 0x1174, 1.0f, EAX, ctx);
				});

				CameraFOVInstruction12Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV12], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.ecx + 0x508, 1.0f, EDX, ctx);
				});

				CameraFOVInstruction13Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV13], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0x1190, 1.0f, ECX, ctx);
				});

				CameraFOVInstruction14Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV14], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0x11AC, 1.0f, ECX, ctx);
				});

				CameraFOVInstruction15Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV15], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.ecx + 0x1270, 1.0f, EDX, ctx);
				});

				CameraFOVInstruction16Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV16], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.eax + 0x1098, 1.0f, ECX, ctx);
				});

				CameraFOVInstruction17Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV17], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.edx + 0x10B4, 1.0f, EAX, ctx);
				});

				CameraFOVInstruction18Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV18], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.ecx + 0xF80, 1.0f, EDX, ctx);
				});

				CameraFOVInstruction19Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV19], [](SafetyHookContext& ctx)
				{
					CameraFOVInstructionsMidHook(ctx.edx + 0xC84, 1.0f, EAX, ctx);
				});

				fNewHUDFOV = Maths::CalculateNewFOV_RadBased(fOriginalHUDFOV, fAspectRatioScale);

				Memory::Write(CameraFOVInstructionsScansResult[FOV20] + 3, fNewHUDFOV);
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