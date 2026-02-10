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
HMODULE dllModule = nullptr;
HMODULE dllModule2;
HMODULE thisModule;

// Fix details
std::string sFixName = "Hidden&DangerousDeluxeFOVFix";
std::string sFixVersion = "1.4";
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
float fRenderingDistanceFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fNewAspectRatio2;
float fAspectRatioScale;
float fCurrentCameraFOV1;
float fNewCameraFOV1;
float fNewCameraFOV2;
float fNewRenderingDistance1;
float fNewRenderingDistance2;

// Game detection
enum class Game
{
	HDD,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2
};

enum RenderingDistanceInstructionsIndices
{
	RenderingDistance1,
	RenderingDistance2,
	RenderingDistance3,
	RenderingDistance4,
	RenderingDistance5,
	RenderingDistance6
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::HDD, {"Hidden & Dangerous Deluxe", "hde.exe"}},
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
		// Use std::filesystem::path operations for clarity
		std::filesystem::path logFilePath = sExePath / sLogFile;
		logger = spdlog::basic_logger_st(sFixName, logFilePath.string(), true);
		spdlog::set_default_logger(logger);
		spdlog::flush_on(spdlog::level::debug);
		spdlog::set_level(spdlog::level::debug); // Enable debug level logging

		spdlog::info("----------");
		spdlog::info("{:s} v{:s} loaded.", sFixName, sFixVersion);
		spdlog::info("----------");
		spdlog::info("Log file: {}", logFilePath.string());
		spdlog::info("----------");
		spdlog::info("Module Name: {:s}", sExeName);
		spdlog::info("Module Path: {:s}", sExePath.string());
		spdlog::info("Module Address: 0x{:X}", (uintptr_t)exeModule);
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
	std::filesystem::path configPath = sFixPath / sConfigFile;
	std::ifstream iniFile(configPath);
	if (!iniFile)
	{
		AllocConsole();
		FILE* dummy;
		freopen_s(&dummy, "CONOUT$", "w", stdout);
		std::cout << sFixName << " v" << sFixVersion << " loaded." << std::endl;
		std::cout << "ERROR: Could not locate config file." << std::endl;
		std::cout << "ERROR: Make sure " << sConfigFile << " is located in " << sFixPath.string() << std::endl;
		spdlog::shutdown();
		FreeLibraryAndExitThread(thisModule, 1);
	}
	else
	{
		spdlog::info("Config file: {}", configPath.string());
		ini.parse(iniFile);
	}

	// Parse config
	ini.strip_trailing_comments();
	spdlog::info("----------");

	// Load settings from ini
	inipp::get_value(ini.sections["FOVFix"], "Enabled", bFixActive);
	spdlog_confparse(bFixActive);

	// Load new INI entries
	inipp::get_value(ini.sections["Settings"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Settings"], "Height", iCurrentResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	inipp::get_value(ini.sections["Settings"], "RenderingDistanceFactor", fRenderingDistanceFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fRenderingDistanceFactor);

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

	dllModule2 = Memory::GetHandle("i3d2.dll");

	return true;
}

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid RenderingDistanceInstruction1Hook{};
static SafetyHookMid RenderingDistanceInstruction2Hook{};
static SafetyHookMid RenderingDistanceInstruction3Hook{};
static SafetyHookMid RenderingDistanceInstruction4Hook{};
static SafetyHookMid RenderingDistanceInstruction5Hook{};
static SafetyHookMid RenderingDistanceInstruction6Hook{};

enum destInstruction
{
	FLD,
	FDIVR
};

void RenderingDistanceInstructions1MidHook(uintptr_t RendDistanceAddress, destInstruction DestInstruction)
{
	float& fCurrentRenderingDistance1 = *reinterpret_cast<float*>(RendDistanceAddress);

	if (fCurrentCameraFOV1 == 1.483529806f * fFOVFactor && fCurrentRenderingDistance1 == 50.0f)
	{
		fNewRenderingDistance1 = fCurrentRenderingDistance1 * fRenderingDistanceFactor;
	}
	else
	{
		fNewRenderingDistance1 = fCurrentRenderingDistance1;
	}

	switch (DestInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewRenderingDistance1);
			break;
		}

		case FDIVR:
		{
			FPU::FDIVR(fNewRenderingDistance1);
			break;
		}
	}	
}

void RenderingDistanceInstructions2MidHook(uintptr_t RendDistanceAddress, uintptr_t& DestInstruction)
{
	float& fCurrentRenderingDistance2 = *reinterpret_cast<float*>(RendDistanceAddress);

	fNewRenderingDistance2 = fCurrentRenderingDistance2 * fRenderingDistanceFactor;

	DestInstruction = std::bit_cast<uintptr_t>(fNewRenderingDistance2);
}

void FOVFix()
{
	if (eGameType == Game::HDD && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "C7 80 ?? ?? ?? ?? ?? ?? ?? ?? 83 E1");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is i3d2.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			fNewAspectRatio2 = 0.75f / fAspectRatioScale;

			Memory::Write(AspectRatioInstructionScanResult + 6, fNewAspectRatio2);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 45 ?? 8B 81", exeModule, "68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 54 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is i3d2.dll+{:x}", CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 3);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				fCurrentCameraFOV1 = *reinterpret_cast<float*>(ctx.ebp + 0xC);

				fNewCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, fAspectRatioScale);

				FPU::FLD(fNewCameraFOV1);
			});

			fNewCameraFOV2 = 1.0f * fFOVFactor;

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 1, fNewCameraFOV2);
		}

		std::vector<std::uint8_t*> RenderingDistanceInstructionsScansResult = Memory::PatternScan(dllModule2, "D9 80 ?? ?? ?? ?? D9 5D", "D9 83 ?? ?? ?? ?? D8 A3 ?? ?? ?? ?? D8 BB ?? ?? ?? ?? D9 5D", "D8 BB ?? ?? ?? ?? D9 5D", "8B 90 ?? ?? ?? ?? 8D 98", "8B 97 ?? ?? ?? ?? 8B 1E", "8B 80 ?? ?? ?? ?? 89 45");
		if (Memory::AreAllSignaturesValid(RenderingDistanceInstructionsScansResult) == true)
		{
			spdlog::info("Rendering Distance Instruction 1: Address is i3d2.dll+{:x}", RenderingDistanceInstructionsScansResult[RenderingDistance1] - (std::uint8_t*)dllModule2);

			spdlog::info("Rendering Distance Instruction 2: Address is i3d2.dll+{:x}", RenderingDistanceInstructionsScansResult[RenderingDistance2] - (std::uint8_t*)dllModule2);

			spdlog::info("Rendering Distance Instruction 3: Address is i3d2.dll+{:x}", RenderingDistanceInstructionsScansResult[RenderingDistance3] - (std::uint8_t*)dllModule2);

			spdlog::info("Rendering Distance Instruction 4: Address is i3d2.dll+{:x}", RenderingDistanceInstructionsScansResult[RenderingDistance4] - (std::uint8_t*)dllModule2);

			spdlog::info("Rendering Distance Instruction 5: Address is i3d2.dll+{:x}", RenderingDistanceInstructionsScansResult[RenderingDistance5] - (std::uint8_t*)dllModule2);

			spdlog::info("Rendering Distance Instruction 5: Address is i3d2.dll+{:x}", RenderingDistanceInstructionsScansResult[RenderingDistance6] - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(RenderingDistanceInstructionsScansResult[RenderingDistance1], 6);

			RenderingDistanceInstruction1Hook = safetyhook::create_mid(RenderingDistanceInstructionsScansResult[RenderingDistance1], [](SafetyHookContext& ctx)
			{
				RenderingDistanceInstructions1MidHook(ctx.eax + 0x1EC, FLD);
			});

			Memory::WriteNOPs(RenderingDistanceInstructionsScansResult[RenderingDistance2], 6);

			RenderingDistanceInstruction2Hook = safetyhook::create_mid(RenderingDistanceInstructionsScansResult[RenderingDistance2], [](SafetyHookContext& ctx)
			{
				RenderingDistanceInstructions1MidHook(ctx.ebx + 0x1EC, FLD);
			});

			Memory::WriteNOPs(RenderingDistanceInstructionsScansResult[RenderingDistance3], 6);

			RenderingDistanceInstruction3Hook = safetyhook::create_mid(RenderingDistanceInstructionsScansResult[RenderingDistance3], [](SafetyHookContext& ctx)
			{
				RenderingDistanceInstructions1MidHook(ctx.ebx + 0x1EC, FDIVR);
			});

			Memory::WriteNOPs(RenderingDistanceInstructionsScansResult[RenderingDistance4], 6);

			RenderingDistanceInstruction4Hook = safetyhook::create_mid(RenderingDistanceInstructionsScansResult[RenderingDistance4], [](SafetyHookContext& ctx)
			{
				RenderingDistanceInstructions2MidHook(ctx.eax + 0x1EC, ctx.edx);
			});

			Memory::WriteNOPs(RenderingDistanceInstructionsScansResult[RenderingDistance5], 6);

			RenderingDistanceInstruction5Hook = safetyhook::create_mid(RenderingDistanceInstructionsScansResult[RenderingDistance5], [](SafetyHookContext& ctx)
			{
				RenderingDistanceInstructions2MidHook(ctx.edi + 0x1EC, ctx.edx);
			});

			Memory::WriteNOPs(RenderingDistanceInstructionsScansResult[RenderingDistance6], 6);

			RenderingDistanceInstruction6Hook = safetyhook::create_mid(RenderingDistanceInstructionsScansResult[RenderingDistance6], [](SafetyHookContext& ctx)
			{
				RenderingDistanceInstructions2MidHook(ctx.eax + 0x1EC, ctx.eax);
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
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH: {
		thisModule = hModule;
		HANDLE mainHandle = CreateThread(NULL, 0, Main, 0, NULL, 0);
		if (mainHandle) {
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