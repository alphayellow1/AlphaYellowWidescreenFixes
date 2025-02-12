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
std::string sFixName = "TheHistoryChannelCivilWarANationDividedFOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float epsilon = 0.00001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fFOVFactor;

// Game detection
enum class Game
{
	THCCWAND,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::THCCWAND, {"The History Channel: Civil War - A Nation Divided", "cw.exe"}},
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
		}
		else
		{
			spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
			return false;
		}
	}

	while (!dllModule2)
	{
		dllModule2 = GetModuleHandleA("CloakNTEngine.dll");
		spdlog::info("Waiting for CloakNTEngine.dll to load...");
		Sleep(1000);
	}

	spdlog::info("Successfully obtained handle for CloakNTEngine.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule2));

	return true;
}

float CalculateNewFOV(float fCurrentFOV)
{
	return fFOVFactor * (2.0f * atanf(tanf(fCurrentFOV / 2.0f) * (fNewAspectRatio / fOldAspectRatio)));
}

SafetyHookMid CameraFOVHipfireLevelStartHook{};

static float lastModifiedFOV = 0.0f;

void CameraFOVHipfireLevelStartMidHook(SafetyHookContext& ctx)
{
	_asm
	{
		push dword ptr ds : [esi + 0x8]
		push eax
		mov dword ptr ds : [esi + 0x8] , [0x102F8538]
	}

	float& fCurrentFOVValue = *reinterpret_cast<float*>(ctx.esi + 0x8);

	fCurrentFOVValue = CalculateNewFOV(1.2200000286102295f);

	_asm
	{
		mov eax, [esi + 0x8]
		mov dword ptr ds : [0x102F8538] , eax
		pop dword ptr ds : [esi + 0x8]
		pop eax
	}
}

void FOVFix()
{
	if (eGameType == Game::THCCWAND && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraFOVStartInstructionScanResult = Memory::PatternScan(dllModule2, "8B 0D ?? ?? ?? ?? 89 86 78 02 00 00 8B D1");
		if (CameraFOVStartInstructionScanResult)
		{
			spdlog::info("Camera FOV Start Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVStartInstructionScanResult - (std::uint8_t*)dllModule2);

			CameraFOVHipfireLevelStartHook = safetyhook::create_mid(CameraFOVStartInstructionScanResult, CameraFOVHipfireLevelStartMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV start instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVZoomInstructionScanResult = Memory::PatternScan(dllModule2, "89 91 B0 00 00 00 C6 81 E0 00 00 00 01 C2 04 00");
		if (CameraFOVZoomInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is CloakNTEngine.dll+{:x}", CameraFOVZoomInstructionScanResult - (std::uint8_t*)dllModule2);

			static SafetyHookMid CameraFOVZoomInstructionMidHook{};

			CameraFOVZoomInstructionMidHook = safetyhook::create_mid(CameraFOVZoomInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float fCurrentZoomFOV = std::bit_cast<float>(ctx.edx);

				if (fCurrentZoomFOV == 1.2200000286102295f)
				{
					fCurrentZoomFOV = CalculateNewFOV(1.2200000286102295f);
				}
				else
				{
					fCurrentZoomFOV = CalculateNewFOV(0.6899999976158142f);
				}

				ctx.edx = std::bit_cast<uintptr_t>(fCurrentZoomFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
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