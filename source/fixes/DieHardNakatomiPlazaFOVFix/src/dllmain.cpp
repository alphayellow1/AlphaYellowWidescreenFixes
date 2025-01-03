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
#include <cmath> // For atan, tan
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
std::string sFixVersion = "1.2"; // Updated version
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
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;
constexpr float epsilon = 0.00001f;

// Ini variables
bool FixActive;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixActive);
	spdlog_confparse(FixActive);

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
	if (eGameType == Game::DHNP && FixActive == true) {
		std::uint8_t* DHNP_HFOVScanResult = Memory::PatternScan(exeModule, "8B 81 98 01 00 00");
		if (DHNP_HFOVScanResult) {
			spdlog::info("HFOV: Address is {:s}+{:x}", sExeName.c_str(), DHNP_HFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid DHNP_HFOVMidHook{};
			DHNP_HFOVMidHook = safetyhook::create_mid(DHNP_HFOVScanResult,
				[](SafetyHookContext& ctx) {
				if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5707963705062866f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5700000524520874f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.5585054159164429f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.4886922240257263f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.0471975803375244f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.5235987901687622f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.3490658700466156f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.8726646900177002f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.1745329350233078f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.1344640254974365f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.2617993950843811f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.06981317698955536f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.13962635397911072f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.3962634801864624f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.7853981852531433f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.2217305898666382f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.6981317400932312f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5312644243240356f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.4747148752212524f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.448533296585083f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.4223518371582031f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.3919837474822998f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.3613520860671997f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.3307284116744995f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.3045469522476196f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.2783653736114502f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.2521837949752808f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.2260023355484009f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.1953785419464111f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.1691970825195312f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.1430155038833618f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.1168339252471924f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.0906524658203125f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.0602843761444092f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.4363323152065277f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
				else if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.4363323152065277f) {
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
			});

			std::uint8_t* DHNP_VFOVScanResult = Memory::PatternScan(exeModule, "8B 81 9C 01 00 00");
			if (DHNP_VFOVScanResult) {
				spdlog::info("VFOV: Address is {:s}+{:x}", sExeName.c_str(), DHNP_VFOVScanResult - (std::uint8_t*)exeModule);
				static SafetyHookMid DHNP_VFOVMidHook{};
				DHNP_VFOVMidHook = safetyhook::create_mid(DHNP_VFOVScanResult,
					[](SafetyHookContext& ctx) {
					if (fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1780972480773926f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon)
					{
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1780972480773926f;
					}
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
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.39269909262657166f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.39269909262657166f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.2617994248867035f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.2617994248867035f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.6544985175132751f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.6544985175132751f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.13089971244335175f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.13089971244335175f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8508480191230774f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8508480191230774f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.19634954631328583f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.19634954631328583f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.05235988646745682f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.05235988646745682f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.10471977293491364f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.10471977293491364f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.41887906193733215f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.41887906193733215f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.39269909262657166f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.39269909262657166f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.047197699546814f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.047197699546814f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.7853982448577881f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.7853982448577881f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.5890486240386963f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.5890486240386963f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.9162979125976562f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.9162979125976562f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.523598849773407f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.523598849773407f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1484483480453491f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1484483480453491f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1060361862182617f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1060361862182617f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0863999128341675f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0863999128341675f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0667638778686523f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0667638778686523f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0439878702163696f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0439878702163696f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.0210140943527222f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.0210140943527222f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.998046338558197f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.998046338558197f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.9784102439880371f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.9784102439880371f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.9587740302085876f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.9587740302085876f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.939137876033783f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.939137876033783f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.919501781463623f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.919501781463623f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8965339064598083f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8965339064598083f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8768978118896484f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8768978118896484f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8572616577148438f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8572616577148438f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8376254439353943f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8376254439353943f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.8179893493652344f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.8179893493652344f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.7952132821083069f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.7952132821083069f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1780973672866821f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1780973672866821f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1780972480773926f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1780972480773926f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.3272492289543152f) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.3272492289543152f;
					}
				});
			}
		}

		std::uint8_t* DHNP_ZoomHFOVScanResult = Memory::PatternScan(exeModule, "89 81 98 01 00 00 8B 45 10");
		if (DHNP_ZoomHFOVScanResult) {
			spdlog::info("HFOV Zoom: Address is {:s}+{:x}", sExeName.c_str(), DHNP_ZoomHFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid DHNP_ZoomHFOVMidHook{};
			DHNP_ZoomHFOVMidHook = safetyhook::create_mid(DHNP_ZoomHFOVScanResult,
				[](SafetyHookContext& ctx) {
				if (ctx.eax == std::bit_cast<uint32_t>(0.4363323152065277f))
				{
					ctx.eax = std::bit_cast<uint32_t>(2.0f * atanf(tanf(0.4363323152065277f / 2.0f) * ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f))));
				}
			});
		}

		std::uint8_t* DHNP_ZoomVFOVScanResult = Memory::PatternScan(exeModule, "89 81 9C 01 00 00 5D C3");
		if (DHNP_ZoomVFOVScanResult) {
			spdlog::info("VFOV Zoom: Address is {:s}+{:x}", sExeName.c_str(), DHNP_ZoomVFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid DHNP_ZoomVFOVMidHook{};
			DHNP_ZoomVFOVMidHook = safetyhook::create_mid(DHNP_ZoomVFOVScanResult,
				[](SafetyHookContext& ctx) {
				if (ctx.eax == std::bit_cast<uint32_t>(0.3272492289543152f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f))))
				{
					ctx.eax = std::bit_cast<uint32_t>(0.3272492289543152f);
				}
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