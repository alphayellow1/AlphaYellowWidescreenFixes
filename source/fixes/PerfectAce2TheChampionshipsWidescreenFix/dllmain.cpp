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
std::string sFixName = "PerfectAce2TheChampionshipsWidescreenFix";
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
float fNewAspectRatio2;
float fNewGameplayFOV;
float fNewMainMenuFOV;
float fNewCutscenesFOV;
int iNewViewportResWidth2;
int iNewViewportResHeight2;
int iNewViewportResWidth3;
int iNewViewportResHeight3;

// Game detection
enum class Game
{
	PA2TC,
	Unknown
};

enum ResolutionInstructionsIndices
{
	RendererResScan,
	ViewportRes1Scan,
	ViewportRes2Scan,
	ViewportRes3Scan
};

enum AspectRatioInstructionsIndices
{
	GameplayARScan,
	CutscenesARScan
};

enum CameraFOVInstructionsIndices
{
	HFOVScan,
	VFOVScan,
	MainMenuFOVScan,
	CutscenesFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::PA2TC, {"Perfect Ace 2: The Championships", "ACEPC.exe"}},
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

static SafetyHookMid RendererResolutionWidthInstructionHook{};
static SafetyHookMid RendererResolutionHeightInstructionHook{};
static SafetyHookMid ViewportResolutionWidthInstruction1Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction1Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction2Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction2Hook{};
static SafetyHookMid ViewportResolutionWidthInstruction3Hook{};
static SafetyHookMid ViewportResolutionHeightInstruction3Hook{};
static SafetyHookMid GameplayAspectRatioInstructionHook{};
static SafetyHookMid CutscenesAspectRatioInstructionHook{};
static SafetyHookMid CameraHFOVInstructionHook{};
static SafetyHookMid CameraVFOVInstructionHook{};
static SafetyHookMid MainMenuFOVInstructionHook{};
static SafetyHookMid CutscenesFOVInstructionHook{};

void GameplayFOVInstructionsMidHook(SafetyHookContext& ctx)
{
	float& fCurrentGameplayFOV = *reinterpret_cast<float*>(ctx.ecx + ctx.eax * 0x4 + 0xC0);

	fNewGameplayFOV = fCurrentGameplayFOV * fAspectRatioScale * fFOVFactor;

	FPU::FMUL(fNewGameplayFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::PA2TC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "8B 0F 89 0D", "89 15 ?? ?? ?? ?? 8D 14", "89 46 ?? 89 44 24 ?? 8B 86", "8B 48 ?? 56 89 4E");

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "D8 B4 81", "8D 4C 24 ?? 51 56 E8 ?? ?? ?? ?? 8D 54 24 ?? 52 56 E8 ?? ?? ?? ?? 8B 44 24");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "D8 8C 81 ?? ?? ?? ?? 52", "D8 8C 81 ?? ?? ?? ?? D8 B4 81", "8B 4C 24 ?? 53 51 B9", "D9 86 ?? ?? ?? ?? 8B 85");

		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[RendererResScan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportRes1Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportRes2Scan] - (std::uint8_t*)exeModule);
			
			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[ViewportRes3Scan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(ResolutionInstructionsScansResult[RendererResScan], 2);

			RendererResolutionWidthInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResScan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[RendererResScan] + 8, 3);

			RendererResolutionHeightInstructionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[RendererResScan] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ViewportResolutionWidthInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportRes1Scan], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResX);
			});

			ViewportResolutionHeightInstruction1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportRes1Scan] + 9, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportRes2Scan], 3);

			ViewportResolutionWidthInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportRes2Scan], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x8) = iCurrentResX;
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportRes2Scan] + 18, 3);

			ViewportResolutionHeightInstruction2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportRes2Scan] + 18, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = iNewViewportResHeight2;
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportRes3Scan], 3);

			ViewportResolutionWidthInstruction3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportRes3Scan], [](SafetyHookContext& ctx)
			{
				int& iCurrentViewportResWidth3 = *reinterpret_cast<int*>(ctx.eax + 0x8);

				int& iCurrentViewportResHeight3 = *reinterpret_cast<int*>(ctx.eax + 0xC);

				const float& fCurrentAspect = static_cast<float>(iCurrentViewportResWidth3) / static_cast<float>(iCurrentViewportResHeight3);

				if (Maths::isCloseRel(fCurrentAspect, 0.805704116821f * fAspectRatioScale) || Maths::isCloseRel(fCurrentAspect, 0.764705896378f * fAspectRatioScale))
				{
					iNewViewportResWidth3 = iCurrentViewportResWidth3;
				}
				else
				{
					iNewViewportResWidth3 = iCurrentResX;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(iNewViewportResWidth3);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ViewportRes3Scan] + 7, 3);

			ViewportResolutionHeightInstruction3Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[ViewportRes3Scan] + 7, [](SafetyHookContext& ctx)
			{
				int& iCurrentViewportResWidth3 = *reinterpret_cast<int*>(ctx.eax + 0x8);

				int& iCurrentViewportResHeight3 = *reinterpret_cast<int*>(ctx.eax + 0xC);

				const float& fCurrentAspect = static_cast<float>(iCurrentViewportResWidth3) / static_cast<float>(iCurrentViewportResHeight3);

				if (Maths::isCloseRel(fCurrentAspect, 0.805704116821f * fAspectRatioScale) || Maths::isCloseRel(fCurrentAspect, 0.764705896378f * fAspectRatioScale))
				{
					iNewViewportResHeight3 = iCurrentViewportResHeight3;
				}
				else
				{
					iNewViewportResHeight3 = iCurrentResY;
				}

				ctx.edx = std::bit_cast<uintptr_t>(iNewViewportResHeight3);
			});
		}

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[GameplayARScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[CutscenesARScan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(AspectRatioInstructionsScansResult[GameplayARScan], 7);

			GameplayAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[GameplayARScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = *reinterpret_cast<float*>(ctx.ecx + ctx.eax * 0x4 + 0x11C);

				fNewAspectRatio2 = fCurrentAspectRatio * fAspectRatioScale;

				FPU::FDIV(fNewAspectRatio2);
			});

			CutscenesAspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionsScansResult[CutscenesARScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesAspectRatio = *reinterpret_cast<float*>(ctx.esp + 0x14);

				fCurrentCutscenesAspectRatio = fCurrentCutscenesAspectRatio * fAspectRatioScale;
			});
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[HFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[VFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Main Menu FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[MainMenuFOVScan] - (std::uint8_t*)exeModule);

			spdlog::info("Cutscenes FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CutscenesFOVScan] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[HFOVScan], 7);

			CameraHFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[HFOVScan], GameplayFOVInstructionsMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[VFOVScan], 7);

			CameraVFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[VFOVScan], GameplayFOVInstructionsMidHook);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MainMenuFOVScan], 4);

			MainMenuFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MainMenuFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentMainMenuFOV = *reinterpret_cast<float*>(ctx.esp + 0x14);

				fNewMainMenuFOV = fCurrentMainMenuFOV / fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewMainMenuFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[CutscenesFOVScan], 6);

			CutscenesFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV = *reinterpret_cast<float*>(ctx.esi + 0xA4);

				fNewCutscenesFOV = (fCurrentCutscenesFOV / fAspectRatioScale) / fFOVFactor;

				FPU::FLD(fNewCutscenesFOV);
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