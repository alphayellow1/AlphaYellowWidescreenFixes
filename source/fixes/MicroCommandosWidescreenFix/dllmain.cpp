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

// Fix details
std::string sFixName = "MicroCommandosWidescreenFix";
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

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fNewCameraFOV;
float fNewCameraFOV2;

// Game detection
enum class Game
{
	MC,
	Unknown
};

enum AspectRatioInstructionsIndex
{
	AspectRatio1Scan,
	AspectRatio2Scan,
	AspectRatio3Scan,
	AspectRatio4Scan
};

enum CameraFOVInstructionsIndex
{
	CameraFOV1Scan,
	CameraFOV2Scan,
	CameraFOV3Scan,
	CameraFOV4Scan,
	CameraFOV5Scan,
	CameraFOV6Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::MC, {"Micro Commandos", "Micro_Commandos.exe"}},
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

static SafetyHookMid AspectRatioInstruction1Hook{};
static SafetyHookMid AspectRatioInstruction2Hook{};
static SafetyHookMid AspectRatioInstruction3Hook{};
static SafetyHookMid AspectRatioInstruction4Hook{};

void AspectRatioInstructionMidHook(SafetyHookContext& ctx)
{
	FPU::FMUL(fNewAspectRatio);
}

static SafetyHookMid CameraFOVInstruction1Hook{};
static SafetyHookMid CameraFOVInstruction2Hook{};
static SafetyHookMid CameraFOVInstruction3Hook{};
static SafetyHookMid CameraFOVInstruction4Hook{};
static SafetyHookMid CameraFOVInstruction5Hook{};
static SafetyHookMid CameraFOVInstruction6Hook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.esi + 0x9C);

	fNewCameraFOV = fCurrentCameraFOV * fFOVFactor;

	FPU::FLD(fNewCameraFOV);
}

void WidescreenFix()
{
	if (eGameType == Game::MC && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "BF 58 02 00 00 BE 20 03 00 00 57 56 6A 01 E8");

		std::vector<std::uint8_t*> AspectRatioInstructionsScansResult = Memory::PatternScan(exeModule, "8B 86 ?? ?? ?? ?? 51 8B 8E ?? ?? ?? ?? 52 50 51 8D 4E", "D8 8E ?? ?? ?? ?? D8 C9", "D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 51", "D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 52");
		
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(exeModule, "8B 8E ?? ?? ?? ?? 52 50 51 8D 4E", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 15", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8D 54 24", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C", "D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 51", "D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C ?? 52");

		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioInstructionsScansResult[AspectRatio4Scan] - (std::uint8_t*)exeModule);
			
			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio1Scan], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(fNewAspectRatio);
			});

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio2Scan], AspectRatioInstructionMidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio3Scan], AspectRatioInstructionMidHook);

			Memory::PatchBytes(AspectRatioInstructionsScansResult[AspectRatio4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			AspectRatioInstruction4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AspectRatio4Scan], AspectRatioInstructionMidHook);
		}

		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV1Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV2Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV3Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV4Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV5Scan] - (std::uint8_t*)exeModule);

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionsScansResult[CameraFOV6Scan] - (std::uint8_t*)exeModule);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV1Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV1Scan], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = *reinterpret_cast<float*>(ctx.esi + 0x9C);

				fNewCameraFOV2 = fCurrentCameraFOV2 * fFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(fNewCameraFOV2);
			});

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV2Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV2Scan], CameraFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV3Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV3Scan], CameraFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV4Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction4Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV4Scan], CameraFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV5Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction5Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV5Scan], CameraFOVInstructionMidHook);

			Memory::PatchBytes(CameraFOVInstructionsScansResult[CameraFOV6Scan], "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstruction6Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CameraFOV6Scan], CameraFOVInstructionMidHook);
		}
	}
}

static void PatchResolutionFromIniInDllMain(HMODULE thisModule)
{
	constexpr uintptr_t VA_VERT_OPCODE = 0x00411A77;
	constexpr uintptr_t VA_HORZ_OPCODE = 0x00411A7C;
	constexpr uintptr_t IMAGE_BASE_ASSUMED = 0x00400000u;
	constexpr uintptr_t RVA_VERT_OPCODE = VA_VERT_OPCODE - IMAGE_BASE_ASSUMED;
	constexpr uintptr_t RVA_HORZ_OPCODE = VA_HORZ_OPCODE - IMAGE_BASE_ASSUMED;

	constexpr uint32_t DEFAULT_WIDTH = 1280u;
	constexpr uint32_t DEFAULT_HEIGHT = 720u;

	WCHAR modulePath[MAX_PATH + 1] = { 0 };
	if (!GetModuleFileNameW(thisModule, modulePath, MAX_PATH)) {
		OutputDebugStringW(L"[WIDESCREEN] GetModuleFileNameW failed in DllMain\n");
		return;
	}

	WCHAR* lastSlash = nullptr;
	for (WCHAR* p = modulePath; *p; ++p) if (*p == L'\\' || *p == L'/') lastSlash = p;
	size_t dirLen = lastSlash ? static_cast<size_t>(lastSlash - modulePath + 1) : wcslen(modulePath);

	WCHAR iniPath[MAX_PATH + 1];
	const wchar_t iniName[] = L"MicroCommandosWidescreenFix.ini";
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
	if (enabledBuf[0] == L'1' || enabledBuf[0] == L't' || enabledBuf[0] == L'y' || enabledBuf[0] == L'o') {
		// starts with 1/true/yes/on
		fixEnabled = true;
	}
	else {
		// also accept explicit "0" or "false" as disabled; keep default false
		fixEnabled = false;
	}

	// Log what we read
	{
		wchar_t dbg[256];
		swprintf_s(dbg, L"[WIDESCREEN] Enabled read: '%s' -> %s\n", enabledBuf, fixEnabled ? L"true" : L"false");
		OutputDebugStringW(dbg);
	}

	if (!fixEnabled) {
		OutputDebugStringW(L"[WIDESCREEN] Fix not enabled in INI; skipping early patch.\n");
		return;
	}

	// Read width/height integers
	int width = static_cast<int>(GetPrivateProfileIntW(L"Settings", L"Width", DEFAULT_WIDTH, iniPath));
	int height = static_cast<int>(GetPrivateProfileIntW(L"Settings", L"Height", DEFAULT_HEIGHT, iniPath));

	// If not valid, fallback to screen size
	if (width <= 0 || height <= 0) {
		int sx = GetSystemMetrics(SM_CXSCREEN);
		int sy = GetSystemMetrics(SM_CYSCREEN);
		if (sx > 0 && sy > 0) { width = sx; height = sy; }
		else { width = DEFAULT_WIDTH; height = DEFAULT_HEIGHT; }
	}

	// Debug: print requested resolution
	{
		wchar_t dbg[256];
		swprintf_s(dbg, L"[WIDESCREEN] Requested resolution from INI: %d x %d\n", width, height);
		OutputDebugStringW(dbg);
	}

	// compute addresses using actual exe base (handles ASLR)
	HMODULE exeBase = GetModuleHandleW(nullptr);
	if (!exeBase) {
		OutputDebugStringW(L"[WIDESCREEN] GetModuleHandle(NULL) failed in DllMain\n");
		return;
	}
	uintptr_t baseVA = reinterpret_cast<uintptr_t>(exeBase);

	uint8_t* vertImmPtr = reinterpret_cast<uint8_t*>(baseVA + RVA_VERT_OPCODE) + 1;
	uint8_t* horzImmPtr = reinterpret_cast<uint8_t*>(baseVA + RVA_HORZ_OPCODE) + 1;

	uint32_t newHeight = static_cast<uint32_t>(height);
	uint32_t newWidth = static_cast<uint32_t>(width);

	Memory::Write<uint32_t>(vertImmPtr, newHeight);
	Memory::Write<uint32_t>(horzImmPtr, newWidth);
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

		PatchResolutionFromIniInDllMain(thisModule);

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