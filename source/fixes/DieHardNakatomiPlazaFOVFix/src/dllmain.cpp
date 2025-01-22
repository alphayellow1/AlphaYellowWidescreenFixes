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
#include <unordered_set>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdint>
#include <iostream>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "DieHardNakatomiPlazaFOVFix";
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
constexpr float fOldWidth = 4.0f;
constexpr float fOldHeight = 3.0f;
constexpr float fOldAspectRatio = fOldWidth / fOldHeight;
constexpr float epsilon = 0.00001f;

// Ini variables
bool bFixActive;

// Variables
int iCurrentResX;
int iCurrentResY;

// Game detection
enum class Game
{
	DHNP,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::DHNP, {"Die Hard: Nakatomi Plaza", "Lithtech.exe"}},
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
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);

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

void FOVFix()
{
	if (eGameType == Game::DHNP && bFixActive == true)
	{
		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 98 01 00 00");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};
			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				static const std::unordered_set<float> validAngles =
				{
				1.5707963705062866f, 1.5700000524520874f, 0.5585054159164429f, 0.4886922240257263f,
				1.0471975803375244f, 0.5235987901687622f, 0.3490658700466156f, 0.8726646900177002f,
				0.1745329350233078f, 1.1344640254974365f, 0.2617993950843811f, 0.06981317698955536f,
				0.13962635397911072f, 1.3962634801864624f, 0.7853981852531433f, 1.2217305898666382f,
				0.6981317400932312f, 1.5312644243240356f, 1.4747148752212524f, 1.448533296585083f,
				1.4223518371582031f, 1.3919837474822998f, 1.3613520860671997f, 1.3307284116744995f,
				1.3045469522476196f, 1.2783653736114502f, 1.2521837949752808f, 1.2260023355484009f,
				1.1953785419464111f, 1.1691970825195312f, 1.1430155038833618f, 1.1168339252471924f,
				1.0906524658203125f, 1.0602843761444092f, 0.4363323152065277f
				};

				float* anglePtr = reinterpret_cast<float*>(ctx.ecx + 0x198);
				if (validAngles.find(*anglePtr) != validAngles.end())
				{
					*anglePtr = 2.0f * atanf(tanf(*anglePtr / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio));
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire/cutscenes HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 9C 01 00 00");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};
			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1780972480773926f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio))) < epsilon)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1780972480773926f;
				}
				/*
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.41887909173965454f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.41887909173965454f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.36651918292045593f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.36651918292045593f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.7853981852531433f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.7853981852531433f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.39269909262657166f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.39269909262657166f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.2617994248867035f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.2617994248867035f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.6544985175132751f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.6544985175132751f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.13089971244335175f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.13089971244335175f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8508480191230774f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8508480191230774f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.19634954631328583f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.19634954631328583f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.05235988646745682f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.05235988646745682f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.10471977293491364f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.10471977293491364f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.41887906193733215f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.41887906193733215f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.39269909262657166f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.39269909262657166f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.047197699546814f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.047197699546814f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.7853982448577881f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.7853982448577881f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.5890486240386963f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.5890486240386963f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.9162979125976562f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.9162979125976562f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.523598849773407f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.523598849773407f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1484483480453491f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1484483480453491f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1060361862182617f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1060361862182617f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0863999128341675f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0863999128341675f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0667638778686523f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0667638778686523f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0439878702163696f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0439878702163696f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0210140943527222f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0210140943527222f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.998046338558197f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.998046338558197f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.9784102439880371f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.9784102439880371f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.9587740302085876f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.9587740302085876f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.939137876033783f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.939137876033783f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.919501781463623f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.919501781463623f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8965339064598083f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8965339064598083f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8768978118896484f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8768978118896484f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8572616577148438f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8572616577148438f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8376254439353943f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8376254439353943f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8179893493652344f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8179893493652344f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.7952132821083069f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.7952132821083069f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1780973672866821f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1780973672866821f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1780972480773926f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1780972480773926f;
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.3272492289543152f)
                {
					*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.3272492289543152f;
				}
				*/
			});
		}
		else
		{
			spdlog::error("Failed to locate hipfire/cutscenes VFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraZoomHFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 81 98 01 00 00 8B 45 10");
		if (CameraZoomHFOVInstructionScanResult)
		{
			spdlog::info("Camera Zoom HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraZoomHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraZoomHFOVInstructionMidHook{};
			CameraZoomHFOVInstructionMidHook = safetyhook::create_mid(CameraZoomHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (ctx.eax == std::bit_cast<uint32_t>(0.4363323152065277f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(0.4363323152065277f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio)));
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate weapon zoom HFOV instruction memory address.");
			return;
		}

		std::uint8_t* CameraZoomVFOVInstructionScanResult = Memory::PatternScan(exeModule, "89 81 9C 01 00 00 5D C3");
		if (CameraZoomVFOVInstructionScanResult)
		{
			spdlog::info("Camera Zoom VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraZoomVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraZoomVFOVInstructionMidHook{};
			CameraZoomVFOVInstructionMidHook = safetyhook::create_mid(CameraZoomVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				if (ctx.eax == std::bit_cast<uint32_t>(0.3272492289543152f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / fOldAspectRatio)))
				{
					ctx.eax = std::bit_cast<uint32_t>(0.3272492289543152f);
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate weapon zoom VFOV instruction memory address.");
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