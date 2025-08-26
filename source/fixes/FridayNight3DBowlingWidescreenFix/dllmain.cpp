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
#include <thread>
#include <chrono>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "FridayNight3DBowlingWidescreenFix";
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
double dFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
double dNewCameraFOV;
uint8_t* ResolutionWidthAddress;
uint8_t* ResolutionHeightAddress;
uint8_t* CameraFOVAddress;

// Game detection
enum class Game
{
	FN3DB,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::FN3DB, {"Friday Night 3D Bowling", "bowling.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "FOVFactor", dFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(dFOVFactor);

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

void StartFocusToggleWatcherEnhanced(bool useAltTab = true, int maxWaitMs = 30000, int pollIntervalMs = 100, int delayMs = 300)
{
	std::thread([useAltTab, maxWaitMs, pollIntervalMs, delayMs]() {
		// Helper type for EnumWindows
		struct Finder { DWORD pid; HWND result; };
		Finder finder{};
		finder.pid = GetCurrentProcessId();
		finder.result = nullptr;

		auto enumProc = [](HWND hwnd, LPARAM lParam) -> BOOL
			{
				Finder* f = reinterpret_cast<Finder*>(lParam);
				DWORD wndPid = 0;
				GetWindowThreadProcessId(hwnd, &wndPid);
				if (wndPid != f->pid) return TRUE;               // not our process
				if (GetWindow(hwnd, GW_OWNER) != NULL) return TRUE; // owned window, skip
				if (!IsWindowVisible(hwnd)) return TRUE;         // not visible
				int len = GetWindowTextLengthW(hwnd);
				if (len == 0) return TRUE;                       // skip empty-title windows
				f->result = hwnd;
				return FALSE; // stop enumerating
			};

		auto findMainWindow = [&](DWORD /*pid*/) -> HWND {
			finder.result = nullptr;
			EnumWindows(enumProc, reinterpret_cast<LPARAM>(&finder));
			return finder.result;
			};

		auto SendOneAltTab = []() {
			INPUT inputs[4] = {};
			// Alt down
			inputs[0].type = INPUT_KEYBOARD;
			inputs[0].ki.wVk = VK_MENU;
			// Tab down
			inputs[1].type = INPUT_KEYBOARD;
			inputs[1].ki.wVk = VK_TAB;
			// Tab up
			inputs[2].type = INPUT_KEYBOARD;
			inputs[2].ki.wVk = VK_TAB;
			inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
			// Alt up
			inputs[3].type = INPUT_KEYBOARD;
			inputs[3].ki.wVk = VK_MENU;
			inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
			SendInput(4, inputs, sizeof(INPUT));
			};

		auto AltTabOutThenBack = [&](int dms) {
			SendOneAltTab();
			std::this_thread::sleep_for(std::chrono::milliseconds(dms));
			SendOneAltTab();
			};

		auto MinimizeThenRestoreWindow = [&](HWND hWnd, int dms) {
			if (!hWnd) return;
			ShowWindow(hWnd, SW_MINIMIZE);
			std::this_thread::sleep_for(std::chrono::milliseconds(dms));
			ShowWindow(hWnd, SW_RESTORE);
			};

		// Robust foreground / focus helper
		auto ForceForeground = [&](HWND hWnd) {
			if (!hWnd) return;

			// If already foreground, still try to ensure focus
			HWND hForeground = GetForegroundWindow();
			if (hForeground == hWnd) {
				SetActiveWindow(hWnd);
				SetFocus(hWnd);
				return;
			}

			DWORD foregroundThread = 0;
			DWORD targetThread = 0;
			HWND currentForeground = GetForegroundWindow();
			if (currentForeground)
				foregroundThread = GetWindowThreadProcessId(currentForeground, NULL);
			targetThread = GetWindowThreadProcessId(hWnd, NULL);

			// Try AttachThreadInput between the foreground thread and the target window's thread
			BOOL attached = FALSE;
			if (foregroundThread != 0 && targetThread != 0)
			{
				attached = AttachThreadInput(foregroundThread, targetThread, TRUE);
			}

			// Try to show/raise and set foreground
			ShowWindow(hWnd, SW_RESTORE);
			// Give the window a chance to be visible
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			SetForegroundWindow(hWnd);
			BringWindowToTop(hWnd);
			SetActiveWindow(hWnd);
			SetFocus(hWnd);

			if (attached)
			{
				// detach
				AttachThreadInput(foregroundThread, targetThread, FALSE);
			}

			// If still not foreground, do temporary topmost trick as fallback
			if (GetForegroundWindow() != hWnd)
			{
				// temporarily make it topmost
				SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				SetWindowPos(hWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				// final attempts
				SetForegroundWindow(hWnd);
				BringWindowToTop(hWnd);
				SetActiveWindow(hWnd);
				SetFocus(hWnd);
			}
			};

		int elapsed = 0;
		while (elapsed < maxWaitMs)
		{
			HWND h = findMainWindow(finder.pid);
			if (h)
			{
#ifdef SPDLOG_ACTIVE_LEVEL
				spdlog::info("StartFocusToggleWatcherEnhanced: detected main window HWND=0x{0:X}", (uintptr_t)h);
#endif
				// grace period so the window finishes initialization
				std::this_thread::sleep_for(std::chrono::milliseconds(100));

				if (useAltTab) {
					// perform Alt+Tab out/back
					AltTabOutThenBack(delayMs);

					// After the synthetic Alt+Tab, aggressively force foreground to ensure focus returns.
					// Small sleep to give the system a moment after the second Alt+Tab send.
					std::this_thread::sleep_for(std::chrono::milliseconds(40));
					ForceForeground(h);
				}
				else {
					// minimize/restore and then ensure foreground
					MinimizeThenRestoreWindow(h, delayMs);
					std::this_thread::sleep_for(std::chrono::milliseconds(40));
					ForceForeground(h);
				}

				return; // done
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMs));
			elapsed += pollIntervalMs;
		}

#ifdef SPDLOG_ACTIVE_LEVEL
		spdlog::warn("StartFocusToggleWatcherEnhanced: timed out waiting for main window ({} ms)", maxWaitMs);
#endif
		}).detach();
}

static SafetyHookMid CameraFOVInstructionHook{};

void CameraFOVInstructionMidHook(SafetyHookContext& ctx)
{
	double& dCurrentCameraFOV = *reinterpret_cast<double*>(CameraFOVAddress);

	// Computes the new FOV value
	dNewCameraFOV = Maths::CalculateNewFOV_RadBased(dCurrentCameraFOV, fAspectRatioScale, Maths::FOVRepresentation::VFOVBased) * dFOVFactor;

	_asm
	{
		fld qword ptr ds:[dNewCameraFOV]
	}
}

void WidescreenFix()
{
	if (eGameType == Game::FN3DB && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::uint8_t* ResolutionInstructionsScanResult = Memory::PatternScan(exeModule, "8B 0D E4 86 4C 00 8B 15 E8 86 4C 00 C7 05 D0 84 4C 00 00 00 00 80 89 0D A0 84 4C 00 89 15 A4 84 4C 00");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)exeModule);

			ResolutionWidthAddress = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 24, Memory::PointerMode::Absolute);

			ResolutionHeightAddress = Memory::GetPointer<uint32_t>(ResolutionInstructionsScanResult + 30, Memory::PointerMode::Absolute);

			Memory::PatchBytes(ResolutionInstructionsScanResult, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructionsMidHook{};

			ResolutionInstructionsMidHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			Memory::PatchBytes(ResolutionInstructionsScanResult + 22, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 12);

			static SafetyHookMid ResolutionInstructions2MidHook{};

			ResolutionInstructions2MidHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 22, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ResolutionWidthAddress) = iCurrentResX;

				*reinterpret_cast<int*>(ResolutionHeightAddress) = iCurrentResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioAndCameraFOVInstructionsScanResult = Memory::PatternScan(exeModule, "DD 05 ?? ?? ?? ?? D9 F2 68 AB AA AA 3F 51 8B 4C 24 0C DD D8 D9 1C 24 51");
		if (AspectRatioAndCameraFOVInstructionsScanResult)
		{
			spdlog::info("Aspect Ratio & Camera FOV Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), AspectRatioAndCameraFOVInstructionsScanResult - (std::uint8_t*)exeModule);

			Memory::Write(AspectRatioAndCameraFOVInstructionsScanResult + 9, fNewAspectRatio);

			CameraFOVAddress = Memory::GetPointer<uint32_t>(AspectRatioAndCameraFOVInstructionsScanResult + 2, Memory::PointerMode::Absolute);

			Memory::PatchBytes(AspectRatioAndCameraFOVInstructionsScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(AspectRatioAndCameraFOVInstructionsScanResult, CameraFOVInstructionMidHook);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio and camera FOV instructions scan memory address.");
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
		WidescreenFix();

		// Start watcher: true = Alt+Tab out/back, false = Minimize & Restore
		StartFocusToggleWatcherEnhanced(true /*useAltTab*/, 30000, 100, 300);
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