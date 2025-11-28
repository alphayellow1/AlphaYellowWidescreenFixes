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
std::string sFixName = "MXvsATVUnleashedWidescreenFix";
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
float fNewFramerate;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	MXVSATVU,
	Unknown
};

enum AspectRatioInstructionsIndex
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan,
	AspectRatio4Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MXVSATVU, {"MX vs. ATV Unleashed", "MXvsATV.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "Framerate", fNewFramerate);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fFOVFactor);
	spdlog_confparse(fNewFramerate);

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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::MXVSATVU && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionListScanResult = Memory::PatternScan(exeModule, "C7 00 80 02 00 00 C7 01 E0 01 00 00 C7 05 50 60 75 00 07 00 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 20 03 00 00 C7 00 58 02 00 00 C7 05 50 60 75 00 08 00 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 00 04 00 00 C7 02 00 03 00 00 C7 05 50 60 75 00 09 00 00 00 C3 8B 44 24 08 8B 4C 24 0C C7 00 80 04 00 00 C7 01 60 03 00 00 C7 05 50 60 75 00 0A 00 00 00 C3 8B 54 24 08 8B 44 24 0C C7 02 00 05 00 00 C7 00 00 04 00 00 C7 05 50 60 75 00 0B 00 00 00 C3 8B 4C 24 08 8B 54 24 0C C7 01 40 06 00 00 C7 02 B0 04 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult - (std::uint8_t*)exeModule);

			// 640x480
			Memory::Write(ResolutionListScanResult + 2, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 8, iCurrentResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 33, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 39, iCurrentResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 64, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 70, iCurrentResY);

			// 1152x864
			Memory::Write(ResolutionListScanResult + 95, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 101, iCurrentResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 126, iCurrentResX);

			Memory::Write(ResolutionListScanResult + 132, iCurrentResY);

			// 1600x1200
			Memory::Write(ResolutionListScanResult + 157, iCurrentResX);			

			Memory::Write(ResolutionListScanResult + 163, iCurrentResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list scan memory address.");
			return;
		}

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8B 04 85 ?? ?? ?? ?? 57 8B 3D ?? ?? ?? ?? 57 EB ?? A1 ?? ?? ?? ?? 50 A1 ?? ?? ?? ?? 50 A1 ?? ?? ?? ?? 8B 11", "8B 04 85 ?? ?? ?? ?? 8B 16 51", "8B 0C 8D ?? ?? ?? ?? 51 83 CB", "D8 74 24 ?? 83 C4 ?? 50");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio4Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio1Scan], "\x90\x90\x90\x90\x90\x90\x90", 7);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio1Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio2Scan], "\x90\x90\x90\x90\x90\x90\x90", 7);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio2Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio3Scan], "\x90\x90\x90\x90\x90\x90\x90", 7);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio3Scan], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio4Scan], "\x90\x90\x90\x90", 4);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio4Scan], [](SafetyHookContext& ctx)
			{
				FPU::FDIV(fNewAspectRatio);
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "D9 44 24 ?? 51 D8 0D ?? ?? ?? ?? D9 1C");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90", 4);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esp + 0x8);

				fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;

				FPU::FLD(fNewCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}		
	}
}

static void PatchFramerateFromIniInDllMain(HMODULE thisModule)
{
	constexpr float DEFAULT_FPS = 60.0f;

	WCHAR modulePath[MAX_PATH + 1] = { 0 };
	if (!GetModuleFileNameW(thisModule, modulePath, MAX_PATH)) {
		OutputDebugStringW(L"[WIDESCREEN] GetModuleFileNameW failed in DllMain\n");
		return;
	}

	WCHAR* lastSlash = nullptr;
	for (WCHAR* p = modulePath; *p; ++p) if (*p == L'\\' || *p == L'/') lastSlash = p;
	size_t dirLen = lastSlash ? static_cast<size_t>(lastSlash - modulePath + 1) : wcslen(modulePath);

	WCHAR iniPath[MAX_PATH + 1];
	const wchar_t iniName[] = L"MXvsATVUnleashedWidescreenFix.ini";
	if (dirLen + wcslen(iniName) >= MAX_PATH) {
		OutputDebugStringW(L"[WIDESCREEN] INI path would overflow buffer\n");
		return;
	}
	wmemcpy(iniPath, modulePath, dirLen);
	iniPath[dirLen] = L'\0';
	wcscat_s(iniPath, MAX_PATH + 1, iniName);

	{
		wchar_t dbg[512];
		swprintf_s(dbg, L"[WIDESCREEN] INI path: %s\n", iniPath);
		OutputDebugStringW(dbg);
	}

	WCHAR enabledBuf[32] = { 0 };
	GetPrivateProfileStringW(L"WidescreenFix", L"Enabled", L"true", enabledBuf, (int)_countof(enabledBuf), iniPath);

	// normalize to lowercase and trim leading spaces
	for (WCHAR* p = enabledBuf; *p; ++p) {
		if (*p >= L'A' && *p <= L'Z') *p = static_cast<WCHAR>(*p + (L'a' - L'A'));
	}

	bool fixEnabled = false;
	if (enabledBuf[0] == L'1' || enabledBuf[0] == L't' || enabledBuf[0] == L'y' || enabledBuf[0] == L'o')
	{
		// starts with 1/true/yes/on
		fixEnabled = true;
	}
	else
	{
		// also accept explicit "0" or "false" as disabled; keep default false
		fixEnabled = false;
	}

	if (!fixEnabled)
	{
		OutputDebugStringW(L"[WIDESCREEN] Fix not enabled in INI; skipping early patch.\n");
		return;
	}

	float framerate = static_cast<float>(GetPrivateProfileIntW(L"Settings", L"Framerate", DEFAULT_FPS, iniPath));

	std::uint8_t* FramerateInstructionScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? B9");
	if (FramerateInstructionScanResult)
	{
		Memory::Write(FramerateInstructionScanResult + 1, framerate);
	}
	else
	{
		return;
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

		PatchFramerateFromIniInDllMain(thisModule);

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