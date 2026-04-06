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
std::string sFixName = "SimonTheSorcerer3DWidescreenFix";
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
float fNewCameraHFOV;
float fNewCameraFOV;

// Game detection
enum class Game
{
	SS3D,
	Unknown
};

enum ResolutionInstructionsIndex
{
	Renderer,
	Viewport1,
	Viewport2,
	Viewport3,
	Viewport4,
	ViewportWidth5,
	ViewportWidth6,
	ViewportHeight7,
	Viewport8,
	Viewport9,
	Viewport10,
	Viewport11,
	Viewport12,
	Viewport13,
	Viewport14,
	Viewport15,
	Viewport16
};

enum CameraHFOVInstructionsIndex
{
	HFOV1,
	HFOV2,
	HFOV3,
	HFOV4
};

enum CameraFOVInstructionsIndex
{
	FOV1,
	FOV2,
	FOV3,
	FOV4,
	FOV5,
	FOV6
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SS3D, {"Simon the Sorcerer 3D", "Simon3D.exe"}},
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

static SafetyHookMid RendererResolutionInstructionsHook{};
static SafetyHookMid ViewportResolutionInstructions1Hook{};
static SafetyHookMid ViewportResolutionInstructions2Hook{};
static SafetyHookMid ViewportResolutionInstructions3Hook{};
static SafetyHookMid ViewportResolutionInstructions4Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction5Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction6Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction7Hook{};
static SafetyHookMid ViewportResolutionInstructions8Hook{};
static SafetyHookMid ViewportResolutionInstructions9Hook{};
static SafetyHookMid ViewportResolutionInstructions10Hook{};
static SafetyHookMid ViewportResolutionInstructions11Hook{};
static SafetyHookMid ViewportResolutionInstructions12Hook{};
static SafetyHookMid ViewportResolutionInstructions13Hook{};
static SafetyHookMid ViewportResolutionInstructions14Hook{};
static SafetyHookMid ViewportResolutionInstructions15Hook{};
static SafetyHookMid ViewportResolutionInstructions16Hook{};
static SafetyHookMid CameraHFOVInstruction1Hook{};
static SafetyHookMid CameraHFOVInstruction2Hook{};
static SafetyHookMid CameraHFOVInstruction3Hook{};
static SafetyHookMid CameraHFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};

enum DestInstruction
{
	FLD,
	FADD
};

void CameraHFOVInstructionsMidHook(uintptr_t HFOVAddress, DestInstruction destInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraHFOV = Memory::ReadMem(HFOVAddress);

	fNewCameraHFOV = fCurrentCameraHFOV * fAspectRatioScale;

	switch (destInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewCameraHFOV);
			break;
		}
	
		case FADD:
		{
			FPU::FADD(fNewCameraHFOV);
			break;
		}
	}
}

void CameraFOVInstructionsMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = Memory::ReadMem(ctx.esi + 0xFC);

	fNewCameraFOV = fCurrentCameraFOV / fFOVFactor;

	FPU::FLD(fNewCameraFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::SS3D && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0C 85 ?? ?? ?? ?? 8B 14 85 ?? ?? ?? ?? 89 0D", "8B 14 8D ?? ?? ?? ?? 8B 0C 8D",
		"8B 34 85", "8B 14 85 ?? ?? ?? ?? 8B 04 85", "8B 0C 85 ?? ?? ?? ?? 8B 14 85", "8B 04 95 ?? ?? ?? ?? 50", "8B 04 95 ?? ?? ?? ?? 8B 56", "8B 0C 85 ?? ?? ?? ?? 89 4C 24",
		"8B 04 BD ?? ?? ?? ?? 8B 1E 89 44 24 ?? DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0C BD ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 4C 24 ?? 50 DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 54 24",
		"8B 04 BD ?? ?? ?? ?? 8B 1E 89 44 24 ?? DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0C BD ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 4C 24 ?? 50 DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B D3",
		"8B 0C BD ?? ?? ?? ?? 8B 1E 89 4C 24 ?? DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 14 BD ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 54 24 ?? 50 DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 50",
		"8B 0C BD ?? ?? ?? ?? 8B 1E 89 4C 24 ?? DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 14 BD ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 54 24 ?? 50 DF 6C 24 ?? DC 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 5C 24",
		"8B 1C 85", "8B 0C 85 ?? ?? ?? ?? 52 8B 14 85", "8B 04 95 ?? ?? ?? ?? 89 7C 24 ?? 89 44 24 ?? D8 05", "8B 04 95 ?? ?? ?? ?? 89 7C 24 ?? 89 44 24 ?? 8B 14 95", "8B 04 95 ?? ?? ?? ?? 89 7C 24 ?? 89 44 24 ?? 8B 0C 95");

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 86 ?? ?? ?? ?? D8 4C 24 ?? D9 18",
		"D9 86 ?? ?? ?? ?? D9 C0 D8 4D ?? 51 D9 1C ?? 51 D9 C0 D8 4D ?? D9 1C ?? 51 8D 4C 24 ?? D8 4D ?? D9 1C ?? E8 ?? ?? ?? ?? 50 8D 4C 24 ?? E8 ?? ?? ?? ?? D9 44 24",
		"D8 86 ?? ?? ?? ?? D8 4C 24 ?? D9 58 ?? D9 44 24 ?? D8 86 ?? ?? ?? ?? D8 4C 24 ?? D9 58 ?? D9 44 24",
		"D9 86 ?? ?? ?? ?? D9 C0 D8 4D ?? 51 D9 1C ?? 51 D9 C0 D8 4D ?? D9 1C ?? 51 8D 4C 24 ?? D8 4D ?? D9 1C ?? E8 ?? ?? ?? ?? 50 8D 4C 24 ?? E8 ?? ?? ?? ?? 8D 54 24");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D9 86 ?? ?? ?? ?? D8 4F ?? D9 47 ?? D8 8E ?? ?? ?? ?? DE C1 D9 47 ?? D8 8E ?? ?? ?? ?? DE C1 D9 1C",
		"D9 86 ?? ?? ?? ?? D8 4F ?? 51 DE C1 D9 47 ?? D8 8E ?? ?? ?? ?? DE C1 D9 1C ?? D9 86", "D9 86 ?? ?? ?? ?? D8 4F ?? D9 47 ?? D8 8E ?? ?? ?? ?? 51 8D 4C 24", "D9 86 ?? ?? ?? ?? D9 E0 D9 86 ?? ?? ?? ?? D9 E0 D9 86",
		"D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? DE C1 D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? D9 86", "D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? 89 48");

		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Renderer] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport1] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport2] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport3] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 4 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport4] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Width 5 Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportWidth5] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Width 6 Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportWidth6] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Height 7 Instruction: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportHeight7] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 8 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport8] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 9 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport9] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 10 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport10] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 11 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport11] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 12 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport12] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 13 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport13] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 14 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport14] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 15 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport15] - (std::uint8_t*)exeModule);

			spdlog::info("Viewport Resolution Instructions 16 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Viewport16] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult, Renderer, Viewport4, 0, 14);

			RendererResolutionInstructionsHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Renderer], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ViewportResolutionInstructions1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport1], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ViewportResolutionInstructions2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport2], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ViewportResolutionInstructions3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport3], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ViewportResolutionInstructions4Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport4], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult, ViewportWidth5, ViewportHeight7, 0, 7);

			ViewportResolutionWidthInstruction5Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportWidth5], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			ViewportResolutionWidthInstruction6Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportWidth6], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			ViewportResolutionHeightInstruction7Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportHeight7], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ViewportResolutionInstructions8Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport8], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions9Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport9], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions10Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport10], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions11Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport11], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edi * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions12Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport12], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.eax * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions13Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport13], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.eax * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.eax * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions14Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport14], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edx * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edx * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions15Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport15], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edx * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edx * 0x4 + 0x005D56D0) = iCurrentResY;
			});

			ViewportResolutionInstructions16Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport16], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.edx * 0x4 + 0x005D56BC) = iCurrentResX;

				*reinterpret_cast<int*>(ctx.edx * 0x4 + 0x005D56D0) = iCurrentResY;
			});
		}

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HFOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HFOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HFOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[HFOV4] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult, HFOV1, HFOV4, 0, 6);

			CameraHFOVInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HFOV1], [](SafetyHookContext& ctx)
			{
				CameraHFOVInstructionsMidHook(ctx.esi + 0x160, FADD, ctx);
			});

			CameraHFOVInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HFOV2], [](SafetyHookContext& ctx)
			{
				CameraHFOVInstructionsMidHook(ctx.esi + 0x160, FLD, ctx);
			});

			CameraHFOVInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HFOV3], [](SafetyHookContext& ctx)
			{
				CameraHFOVInstructionsMidHook(ctx.esi + 0x164, FADD, ctx);
			});

			CameraHFOVInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[HFOV4], [](SafetyHookContext& ctx)
			{
				CameraHFOVInstructionsMidHook(ctx.esi + 0x164, FLD, ctx);
			});
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV4] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV5] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV6] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult, FOV1, FOV6, 0, 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], CameraFOVInstructionsMidHook);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], CameraFOVInstructionsMidHook);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], CameraFOVInstructionsMidHook);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV4], CameraFOVInstructionsMidHook);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV5], CameraFOVInstructionsMidHook);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV6], CameraFOVInstructionsMidHook);
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