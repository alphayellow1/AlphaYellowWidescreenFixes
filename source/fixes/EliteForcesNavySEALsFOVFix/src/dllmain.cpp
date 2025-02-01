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
	if (eGameType == Game::EFNS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* CameraHFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 98 01 00 00 89 45 BC");
		if (CameraHFOVInstructionScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraHFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraHFOVInstructionMidHook{};
			CameraHFOVInstructionMidHook = safetyhook::create_mid(CameraHFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Define the set of valid angles
				static const std::unordered_set<float> validAngles = {
					1.6234371662139893f, 1.6211177110671997f, 1.6191377639770508f,
					1.6167963743209839f, 1.6144379377365112f, 1.6124579906463623f,
					1.6107758283615112f, 1.608136534690857f, 1.6057976484298706f,
					1.60347580909729f,  1.6014761924743652f,  1.599176287651062f,
					1.5971375703811646f, 1.5957971811294556f, 1.5928162336349487f,
					1.5907751321792603f, 1.5884557962417603f, 1.586097240447998f,
					1.5841171741485596f, 1.581795334815979f,  1.5794367790222168f,
					1.577434778213501f,  1.5757964849472046f,  1.5727958679199219f,
					1.5707963705062866f, 0.4363323152065277f
				};

				// Access the angle at the specified memory location
				float& angle = *reinterpret_cast<float*>(ctx.ecx + 0x198);

				// Check if the angle is in the set of valid angles
				if (validAngles.find(angle) != validAngles.end()) {
					// Adjust the angle based on the aspect ratio
					angle = 2.0f * atanf(tanf(angle / 2.0f) * (fNewAspectRatio / fOldAspectRatio));
				}
			});
		}

		std::uint8_t* CameraVFOVInstructionScanResult = Memory::PatternScan(exeModule, "8B 81 9C 01 00 00 89 45 C0");
		if (CameraVFOVInstructionScanResult)
		{
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraVFOVInstructionScanResult - (std::uint8_t*)exeModule);

			static SafetyHookMid CameraVFOVInstructionMidHook{};
			CameraVFOVInstructionMidHook = safetyhook::create_mid(CameraVFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				// Define a struct to hold the target value and a condition lambda
				struct ValueCondition
				{
					float targetValue;
					std::function<bool(float)> condition;
				};

				std::vector<ValueCondition> valueConditions =
				{
					{1.1780973672866821f, [&](float value) { return value == 1.1780973672866821f || fabs(value - (1.1780973672866821f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1780972480773926f, [&](float value) { return value == 1.1780972480773926f || fabs(value - (1.1780972480773926f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1757389307022095f, [&](float value) { return value == 1.1757389307022095f || fabs(value - (1.1757389307022095f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1730972528457642f, [&](float value) { return value == 1.1730972528457642f || fabs(value - (1.1730972528457642f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1714589595794678f, [&](float value) { return value == 1.1714589595794678f || fabs(value - (1.1714589595794678f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.169456958770752f, [&](float value) { return value == 1.169456958770752f || fabs(value - (1.169456958770752f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1670984029769897f, [&](float value) { return value == 1.1670984029769897f || fabs(value - (1.1670984029769897f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1647765636444092f, [&](float value) { return value == 1.1647765636444092f || fabs(value - (1.1647765636444092f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1627964973449707f, [&](float value) { return value == 1.1627964973449707f || fabs(value - (1.1627964973449707f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1604379415512085f, [&](float value) { return value == 1.1604379415512085f || fabs(value - (1.1604379415512085f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1581186056137085f, [&](float value) { return value == 1.1581186056137085f || fabs(value - (1.1581186056137085f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.15607750415802f, [&](float value) { return value == 1.15607750415802f || fabs(value - (1.15607750415802f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1530965566635132f, [&](float value) { return value == 1.1530965566635132f || fabs(value - (1.1530965566635132f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1517561674118042f, [&](float value) { return value == 1.1517561674118042f || fabs(value - (1.1517561674118042f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.149397611618042f, [&](float value) { return value == 1.149397611618042f || fabs(value - (1.149397611618042f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1474175453186035f, [&](float value) { return value == 1.1474175453186035f || fabs(value - (1.1474175453186035f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1450175046920776f, [&](float value) { return value == 1.1450175046920776f || fabs(value - (1.1450175046920776f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1430960893630981f, [&](float value) { return value == 1.1430960893630981f || fabs(value - (1.1430960893630981f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1407572031021118f, [&](float value) { return value == 1.1407572031021118f || fabs(value - (1.1407572031021118f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1381179094314575f, [&](float value) { return value == 1.1381179094314575f || fabs(value - (1.1381179094314575f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1364357471466064f, [&](float value) { return value == 1.1364357471466064f || fabs(value - (1.1364357471466064f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1344558000564575f, [&](float value) { return value == 1.1344558000564575f || fabs(value - (1.1344558000564575f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1320973634719849f, [&](float value) { return value == 1.1320973634719849f || fabs(value - (1.1320973634719849f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.129755973815918f, [&](float value) { return value == 1.129755973815918f || fabs(value - (1.129755973815918f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.127776026725769f, [&](float value) { return value == 1.127776026725769f || fabs(value - (1.127776026725769f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{1.1254565715789795f, [&](float value) { return value == 1.1254565715789795f || fabs(value - (1.1254565715789795f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
					{0.3272492289543152f, [&](float value) { return value == 0.3272492289543152f || fabs(value - (0.3272492289543152f / (fNewAspectRatio / fOldAspectRatio))) < epsilon; }},
				};

				// Access the float value at the memory address ecx + 0x19C
				float& floatValue = *reinterpret_cast<float*>(ctx.ecx + 0x19C);

				// Loop through the value conditions
				for (const auto& vc : valueConditions)
				{
					if (vc.condition(floatValue))
					{
						floatValue = vc.targetValue;
						break;
					}
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