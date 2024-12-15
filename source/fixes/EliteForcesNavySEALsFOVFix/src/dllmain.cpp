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
std::string sFixName = "EliteForcesNavySEALsFOVFix";
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
constexpr float fPi = 3.14159265358979323846f;
constexpr float oldWidth = 4.0f;
constexpr float oldHeight = 3.0f;
constexpr float oldAspectRatio = oldWidth / oldHeight;

// Ini variables
bool FixFOV = true;

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

const float epsilon = 0.00001f;

// Game detection
enum class Game
{
	EFNS,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::EFNS, {"Elite Forces: Navy SEALs", "lithtech.exe"}},
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
	inipp::get_value(ini.sections["FOVFix"], "Enabled", FixFOV);
	spdlog_confparse(FixFOV);

	// Load resolution from ini
	inipp::get_value(ini.sections["Resolution"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Resolution"], "Height", iCurrentResY);
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

void FOV()
{
	if (eGameType == Game::EFNS) {
		std::uint8_t* EFNS_HFOVScanResult = Memory::PatternScan(exeModule, "8B 81 98 01 00 00 89 45 BC");
		if (EFNS_HFOVScanResult) {
			spdlog::info("HFOV: Address is {:s}+{:x}", sExeName.c_str(), EFNS_HFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid EFNS_HFOVMidHook{};
			EFNS_HFOVMidHook = safetyhook::create_mid(EFNS_HFOVScanResult,
				[](SafetyHookContext& ctx) {
				if (*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6234371662139893f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6211177110671997f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6191377639770508f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6167963743209839f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6144379377365112f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6124579906463623f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6107758283615112f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.608136534690857f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6057976484298706f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.60347580909729f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.6014761924743652f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.599176287651062f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5971375703811646f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5957971811294556f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5928162336349487f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5907751321792603f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5884557962417603f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.586097240447998f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5841171741485596f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.581795334815979f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5794367790222168f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.577434778213501f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5757964849472046f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5727958679199219f ||
					*reinterpret_cast<float*>(ctx.ecx + 0x198) == 1.5707963705062866f || *reinterpret_cast<float*>(ctx.ecx + 0x198) == 0.4363323152065277f)
				{
					*reinterpret_cast<float*>(ctx.ecx + 0x198) = 2.0f * atanf(tanf(*reinterpret_cast<float*>(ctx.ecx + 0x198) / 2.0f) * ((static_cast<float>(iCurrentResX) / iCurrentResY) / oldAspectRatio));
				}
			});

			std::uint8_t* EFNS_VFOVScanResult = Memory::PatternScan(exeModule, "8B 81 9C 01 00 00 89 45 C0");
			if (EFNS_VFOVScanResult) {
				spdlog::info("VFOV: Address is {:s}+{:x}", sExeName.c_str(), EFNS_VFOVScanResult - (std::uint8_t*)exeModule);
				static SafetyHookMid EFNS_VFOVMidHook{};
				EFNS_VFOVMidHook = safetyhook::create_mid(EFNS_VFOVScanResult,
					[](SafetyHookContext& ctx) {
					if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1780973672866821f || *reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1780972480773926f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1780973672866821f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1780972480773926f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1780973672866821f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1757389307022095f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1757389307022095f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1757389307022095f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1730972528457642f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1730972528457642f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1730972528457642f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1714589595794678f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1714589595794678f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1714589595794678f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.169456958770752f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.169456958770752f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.169456958770752f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1670984029769897f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1670984029769897f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1670984029769897f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1647765636444092f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1647765636444092f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1647765636444092f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1627964973449707f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1627964973449707f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1627964973449707f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1604379415512085f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1604379415512085f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1604379415512085f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1581186056137085f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1581186056137085f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1581186056137085f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.15607750415802f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.15607750415802f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.15607750415802f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1530965566635132f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1530965566635132f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1530965566635132f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1517561674118042f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1517561674118042f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1517561674118042f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.149397611618042f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.149397611618042f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.149397611618042f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1474175453186035f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1474175453186035f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1474175453186035f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1450175046920776f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1450175046920776f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1450175046920776f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1430960893630981f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1430960893630981f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1430960893630981f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1407572031021118f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1407572031021118f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1407572031021118f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1381179094314575f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1381179094314575f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1381179094314575f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1364357471466064f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1364357471466064f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1364357471466064f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1344558000564575f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1344558000564575f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1344558000564575f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1320973634719849f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1320973634719849f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1320973634719849f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.129755973815918f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.129755973815918f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.129755973815918f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.127776026725769f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.127776026725769f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.127776026725769f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 1.1254565715789795f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (1.1254565715789795f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 1.1254565715789795f;
					}
					else if (*reinterpret_cast<float*>(ctx.ecx + 0x19C) == 0.3272492289543152f || fabs(*reinterpret_cast<float*>(ctx.ecx + 0x19C) - (0.3272492289543152f / ((static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY)) / (4.0f / 3.0f)))) < epsilon) {
						*reinterpret_cast<float*>(ctx.ecx + 0x19C) = 0.3272492289543152f;
					}
				});
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
		FOV();
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