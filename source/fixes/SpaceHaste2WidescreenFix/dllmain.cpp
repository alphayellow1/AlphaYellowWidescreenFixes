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

// Fix details
std::string sFixName = "SpaceHaste2WidescreenFix";
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

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
float fNewCameraFOV2;

// Game detection
enum class Game
{
	SH2,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res512x384_1,
	Res512x384_2,
	Res512x384_3,
	Res512x384_4,
	Res640x480,
	Res800x600,
	Res1024x768,
	Res1280x1024,
	Res1600x1200
};

enum CameraFOVInstructionsIndices
{
	FOV1,
	FOV2
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SH2, {"Space Haste 2", "SpaceHaste.exe"}},
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

	dllModule2 = Memory::GetHandle("trend.dll");

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};

enum destInstruction
{
	FLD,
	EAX
};

void CameraFOVInstructionsMidHook(uintptr_t FOVAddress, float arScale, float fovFactor, destInstruction DestInstruction, SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

	fNewCameraFOV = fCurrentCameraFOV * arScale * fovFactor;

	switch (DestInstruction)
	{
		case FLD:
		{
			FPU::FLD(fNewCameraFOV);
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
	if (eGameType == Game::SH2 && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionInstructionsScansResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 7F ?? 74 ?? 3D ?? ?? ?? ?? 74",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 E9", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 EB ?? 8B 15 ?? ?? ?? ?? A1 ?? ?? ?? ?? 52 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 EB ?? 8B 0D ?? ?? ?? ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? EB", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 EB ?? A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 EB ?? 8B 15 ?? ?? ?? ?? 52 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? EB",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 7F ?? 74 ?? 3D ?? ?? ?? ?? 0F 84", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? E9 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 0B C1 8B 0D ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? E9 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 0B C1 8B 0D ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 8F",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? E9 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 0B C1 8B 0D ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? E9 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 0B C1 8B 0D ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 7F",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 8F ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? E9 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 0B C1 8B 0D ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 7F", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 0D ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 55 ?? 52 FF 15 ?? ?? ?? ?? D9 00 E8 ?? ?? ?? ?? 3D ?? ?? ?? ?? 7F ?? 0F 84");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution 512x384 Scan 1: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res512x384_1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 512x384 Scan 2: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res512x384_2] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 512x384 Scan 3: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res512x384_3] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 512x384 Scan 4: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res512x384_4] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 640x480 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res640x480] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 800x600 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res800x600] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 1024x768 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1024x768] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 1280x1024 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1280x1024] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution 1600x1200 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScansResult[Res1600x1200] - (std::uint8_t*)exeModule);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_1] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_1] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_2] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_2] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_3] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_3] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_4] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res512x384_4] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res640x480] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res640x480] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res800x600] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res800x600] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res1024x768] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1024x768] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res1280x1024] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1280x1024] + 1, iCurrentResY);

			Memory::Write(ResolutionInstructionsScansResult[Res1600x1200] + 6, iCurrentResX);

			Memory::Write(ResolutionInstructionsScansResult[Res1600x1200] + 1, iCurrentResY);
		}

		// Both aspect ratio and FOV instructions are located in twnd::SetAspect function
		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "8B 50 04 D8 3D ?? ?? ?? ?? 89 15 ?? ?? ?? ??");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is trend.dll+{:x}", AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 3);			

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(dllModule2, "8B 40 08 A3 ?? ?? ?? ?? 57 33 C0 B9 10 00 00 00 BF ?? ?? ?? ??", exeModule, "D9 45 ?? 8B 0D ?? ?? ?? ?? D8 25 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 05 ?? ?? ?? ?? D9 5D ?? 8B 7D ?? 57 E8 ?? ?? ?? ?? 89 3D");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is trend.dll+{:x}", CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)dllModule2);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)exeModule);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 3);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.eax + 0x8, fAspectRatioScale, 1.0f, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 3);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				CameraFOVInstructionsMidHook(ctx.ebp - 0x4, 1.0f, fFOVFactor, FLD, ctx);
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