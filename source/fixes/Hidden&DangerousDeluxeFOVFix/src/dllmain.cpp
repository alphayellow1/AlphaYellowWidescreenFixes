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
#include "dllmain.h"

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE dllModule = nullptr; // Handle for i3d2.dll
HMODULE thisModule;

// Fix details
std::string sFixName = "Hidden&DangerousDeluxeFOVFix";
std::string sFixVersion = "1.1"; // Updated version
std::filesystem::path sFixPath;

// Ini
inipp::Ini<char> ini;
std::string sConfigFile = sFixName + ".ini";

// Logger
std::shared_ptr<spdlog::logger> logger;
std::string sLogFile = sFixName + ".log";
std::filesystem::path sExePath;
std::string sExeName;

// FOV 
std::pair DesktopDimensions = { 0,0 };
const float fNativeAspect = 4.0f / 3.0f;
float fFOVMultiplier;
float originalValue, newValue;

// Aspect ratio / FOV


// Ini variables
bool bFixFOV = true;

// New INI variables
float fRenderingDistance = 0.5f; // Initialized to 0.5
float fFOVFactor = 1.0f;          // Initialized to 1.0

// Variables
int iCurrentResX = 0;
int iCurrentResY = 0;

// Game detection
enum class Game
{
	HDD,
	Unknown
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

void CalculateFOV(bool bLog)
{
	if (iCurrentResX <= 0 || iCurrentResY <= 0)
		return;

	// Calculate aspect ratio
	float fAspectRatio = (float)iCurrentResX / (float)iCurrentResY;
	fFOVMultiplier = fNativeAspect / fAspectRatio;

	// Log details about current resolution
	if (bLog) {
		spdlog::info("----------");
		spdlog::info("Current Resolution: Resolution: {:d}x{:d}", iCurrentResX, iCurrentResY);
		spdlog::info("Current Resolution: fAspectRatio: {}", fAspectRatio);
		spdlog::info("----------");
	}
}

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
		spdlog::info("Module Address: 0x{:X}", reinterpret_cast<uintptr_t>(exeModule));
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
	inipp::get_value(ini.sections["FOV"], "Enabled", bFixFOV);
	spdlog_confparse(bFixFOV);

	// Load resolution from ini
	inipp::get_value(ini.sections["Resolution"], "Width", iCurrentResX);
	inipp::get_value(ini.sections["Resolution"], "Height", iCurrentResY);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);

	// Load new INI entries
	inipp::get_value(ini.sections["Settings"], "Rendering Distance", fRenderingDistance);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(fRenderingDistance);
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

			dllModule = GetModuleHandleA("i3d2.dll");
			if (!dllModule)
			{
				spdlog::error("Failed to get handle for i3d2.dll.");
				return false;
			}

			spdlog::info("Successfully obtained handle for i3d2.dll: 0x{:X}", reinterpret_cast<uintptr_t>(dllModule));

			return true;
		}
	}

	spdlog::error("Failed to detect supported game, {:s} isn't supported by the fix.", sExeName);
	return false;
}

void FOV()
{


	if (eGameType == Game::HDD) {

		std::uint8_t* HDD_HFOVScanResult = Memory::PatternScan(dllModule, "D8 F1 8B 4D F8 D9 1E");
		if (HDD_HFOVScanResult) {
			spdlog::info("HFOV: Address is {:s}+{:x}", sExeName.c_str(), HDD_HFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid HDD_HFOVMidHook{};
			HDD_HFOVMidHook = safetyhook::create_mid(HDD_HFOVScanResult,
				[](SafetyHookContext& ctx) {

				ctx.fpu.f32[0] = 0.3f;

			});
		}

		/* std::uint8_t* HDD_OverallFOVScanResult = Memory::PatternScan(dllModule, "D9 83 E4 01 00 00 D8 0D ?? ?? ?? ?? D9 C0");
		if (HDD_OverallFOVScanResult) {
			spdlog::info("FOV: Address is {:s}+{:x}", sExeName.c_str(), HDD_OverallFOVScanResult - (std::uint8_t*)exeModule);
			static SafetyHookMid HDD_OverallFOVMidHook{};
			HDD_OverallFOVMidHook = safetyhook::create_mid(HDD_OverallFOVScanResult + 0x9,
				[](SafetyHookContext& ctx) {


				ctx.ebx + 0x1E4;
				spdlog::info("Original Value: {}", originalValue);

				// 2. Multiply it with the value at the provided address
				float multiplyValue = *addressWithValue;
				float result = originalValue * multiplyValue;
				spdlog::info("Multiply Value: {}, Result: {}", multiplyValue, result);

				// 3. Inject the result back into the FPU stack
				__asm {
					fld result  // Push the result onto the FPU stack
				}

				// Continue with the original execution (fdiv and mov)
				ctx.call();

			});
		}*/
	}
}

/*
	// Define the patterns and their corresponding offsets
	PatternInfo patterns[] = {
		{"\xD9\x45\xFC\xD8\xF1\x8B\x4D\xF8", "xxxxxxxx", 8, 0x0000},
		{"\xD9\x83\xE4\x01\x00\x00\xD8\x0D\x9C\x72\x0E\x10\xD9\xC0", "xxxxxxxx????xx", 14, 0x1000},
		{"\xD9\x80\xE4\x01\x00\x00\xD8\x0D\x9C\x72\x0E\x10\x89\x8D\x7C\xFF\xFF\xFF", "xxxxxxxx????xxxxxx", 18, 0x2000},
		{"\xD9\xFE\xD9\x83\xEC\x01\x00\x00\xD8\xA3\xE8\x01\x00\x00", "xxxxxxxxxxxxxx", 14, 0x3000},
	};

	// CustomCode1
	std::vector<Instruction> customCode1 = {
		// Instruction 1
		{{0xEB, 0x05}, false, false, 0, "", false, 0}, // Jump to Instruction 5

		// Instruction 2
		{{0x3D, 0x00, 0x00, 0x40, 0x3F}, false, true, 0, "HFOV", false, 0}, // add byte ptr [eax], al

		// Instruction 5
		{{0x36, 0xD9, 0x45, 0xFC}, false, false, 0, "", false, 0}, // fld dword ptr ss:[ebp - 0x4]

		// Instruction 6
		{{0x3E, 0xD8, 0x0D, 0x03, 0x00, 0x00, 0x00}, false, false, 0, "", true, 3}, // fmul dword ptr ds:[3]

		// Instruction 7
		{{0xD8, 0xF1}, false, false, 0, "", false, 0}, // fdiv st(0),st(1)

		{{0x8B, 0x4D, 0xF8}, false, false, 0, "", false, 0},
	};

	// CustomCode2
	std::vector<Instruction> customCode2 = {
		// Instruction 1
		{{0xEB, 0x05}, false, false, 0, "", false, 0}, // jmp 5

		// Instruction 2
		{{0x3D, 0x00, 0x00, 0x80, 0x3F}, false, false, 0, "FOVFactor", false, 0}, // Float at byte 0

		// Instruction 5
		{{0x3E, 0xD9, 0x83, 0xE4, 0x01, 0x00, 0x00}, false, false, 0, "", false, 0}, // fld dword ptr ds:[ebx+0x1E4]

		// Instruction 6
		{{0x3E, 0xD8, 0x0D, 0x03, 0x00, 0x00, 0x00}, false, false, 0, "", true, 3}, // fmul dword ptr ds:[3]

		// Instruction 7
		{{0xD9, 0xC0}, false, false, 0, "", false, 0}, // fld st(0)
	};

	// CustomCode3
	std::vector<Instruction> customCode3 = {
		// Instruction 1
		{{0xEB, 0x05}, false, false, 0, "", false, 0}, // jmp 4

		// Instruction 2
		{{0x3D, 0x00, 0x00, 0x00, 0x3F}, false, false, 0, "", false, 0}, // add byte ptr [eax], al

		// Instruction 4
		{{0x3E, 0xD9, 0x80, 0xE4, 0x01, 0x00, 0x00}, false, false, 0, "HFOV", false, 0}, // fld dword ptr ds:[eax+0x1E4]

		// Instruction 5
		{{0x3E, 0xD8, 0x0D, 0x03, 0x00, 0x00, 0x00}, false, false, 0, "", true, 3}, // fmul dword ptr ds:[3]

		// Instruction 6
		{{0x89, 0x8D, 0x7C, 0xFF, 0xFF, 0xFF}, false, false, 0, "", false, 0}, // mov dword ptr ss:[ebp - 0x84], ecx
	};

	// CustomCode4
	std::vector<Instruction> customCode4 = {
		// Instruction 1
		{{0xD9, 0xFE}, false, false, 0, "", false, 0}, // fsin

		// Instruction 2
		{{0x3E, 0xC7, 0x83, 0xEC, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x44}, false, false, 0, "RenderingDistance", false, 0}, // Float at byte 6

		// Instruction 3
		{{0x3E, 0xD9, 0x83, 0xEC, 0x01, 0x00, 0x00}, false, false, 0, "", false, 0}, // fld dword ptr ds:[ebx + 0x1EC]

		// Instruction 4
		{{0x3E, 0xD8, 0xA3, 0xE8, 0x01, 0x00, 0x00}, false, false, 0, "", false, 0}, // fsub dword ptr ds:[ebx + 0x1E8]
	};
*/



DWORD __stdcall Main(void*)
{
	Sleep(3000);

	Logging();
	Configuration();
	if (DetectGame()) {
		FOV();
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