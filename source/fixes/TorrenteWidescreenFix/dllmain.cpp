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
HMODULE dllModule2;
HMODULE dllModule3;
HMODULE dllModule4;
HMODULE dllModule5;
HMODULE dllModule6;
HMODULE dllModule7;
HMODULE dllModule8;
HMODULE dllModule9;
HMODULE dllModule10;
HMODULE dllModule11;
HMODULE dllModule12;
HMODULE dllModule13;
HMODULE dllModule14;
HMODULE dllModule15;
HMODULE dllModule16;
HMODULE dllModule17;
HMODULE dllModule18;
HMODULE dllModule19;
HMODULE dllModule20;
HMODULE dllModule21;
HMODULE dllModule22;
HMODULE dllModule23;
HMODULE dllModule24;
HMODULE dllModule25;

// Fix details
std::string sFixName = "TorrenteWidescreenFix";
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
int iNewResX;
int iNewResY;
float fFOVFactor;

// Constants
constexpr float fOldAspectRatio = 4.0f / 3.0f;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	TORRENTE,
	Unknown
};

enum ResolutionInstructionsIndices
{
	ResolutionList1Scan,
	ResolutionList2Scan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::TORRENTE, {"Torrente", "torrente.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "Width", iNewResX);
	inipp::get_value(ini.sections["Settings"], "Height", iNewResY);
	inipp::get_value(ini.sections["Settings"], "FOVFactor", fFOVFactor);
	spdlog_confparse(iNewResX);
	spdlog_confparse(iNewResY);
	spdlog_confparse(fFOVFactor);

	// If resolution not specified, use desktop resolution
	if (iNewResX <= 0 || iNewResY <= 0)
	{
		spdlog::info("Resolution not specified in ini file. Using desktop resolution.");
		// Implement Util::GetPhysicalDesktopDimensions() accordingly
		auto desktopDimensions = Util::GetPhysicalDesktopDimensions();
		iNewResX = desktopDimensions.first;
		iNewResY = desktopDimensions.second;
		spdlog_confparse(iNewResX);
		spdlog_confparse(iNewResY);
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

	dllModule2 = Memory::GetHandle("vtKernel.dll");

	dllModule3 = Memory::GetHandle("Camera.dll");

	dllModule4 = Memory::GetHandle("CocheTorrente.dll");

	dllModule5 = Memory::GetHandle("Comercial.dll");

	dllModule6 = Memory::GetHandle("ContadorDetonador.dll");

	dllModule7 = Memory::GetHandle("ContadorDinero.dll");

	dllModule8 = Memory::GetHandle("ContadorMunicion.dll");

	dllModule9 = Memory::GetHandle("ContadorSalud.dll");

	dllModule10 = Memory::GetHandle("ContadorTiempo.dll");

	dllModule11 = Memory::GetHandle("fase_cuco.dll");

	dllModule12 = Memory::GetHandle("FASE_SPINELLI.dll");

	dllModule13 = Memory::GetHandle("Madrid_F03.dll");

	dllModule14 = Memory::GetHandle("Madrid_F04.dll");

	dllModule15 = Memory::GetHandle("madrid_franco.dll");

	dllModule16 = Memory::GetHandle("madrid_kio.dll");

	dllModule17 = Memory::GetHandle("Main.dll");

	dllModule18 = Memory::GetHandle("marbella_chalets.dll");

	dllModule19 = Memory::GetHandle("Marbella_f01.dll");

	dllModule20 = Memory::GetHandle("Marbella_f02.dll");

	dllModule21 = Memory::GetHandle("Marbella_f03.dll");

	dllModule22 = Memory::GetHandle("Marbella_f04.dll");

	dllModule23 = Memory::GetHandle("marbella_malibu.dll");

	dllModule24 = Memory::GetHandle("Radar.dll");

	dllModule25 = Memory::GetHandle("Script.dll");
	
	return true;
}

static SafetyHookMid CameraFOVInstructionHook{};

using PLDR_DLL_NOTIFICATION_COOKIE = PVOID;
typedef VOID(NTAPI* PLDR_DLL_NOTIFICATION_FUNCTION)(
	ULONG NotificationReason,
	PVOID NotificationData,
	PVOID Context);

enum LDR_DLL_NOTIFICATION_REASON {
	LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
	LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2
};

// Minimal UNICODE_STRING structure
typedef struct _UNICODE_STRING_SIMPLE {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING_SIMPLE, * PUNICODE_STRING_SIMPLE;

// Notification data layout used by LdrRegisterDllNotification callbacks
typedef struct _LDR_DLL_NOTIFICATION_DATA_SIMPLE {
	ULONG Flags;
	PUNICODE_STRING_SIMPLE FullDllName;
	PUNICODE_STRING_SIMPLE BaseDllName;
	PVOID DllBase;
	ULONG SizeOfImage;
} LDR_DLL_NOTIFICATION_DATA_SIMPLE, * PLDR_DLL_NOTIFICATION_DATA_SIMPLE;

// cookie for unregistering
static PLDR_DLL_NOTIFICATION_COOKIE g_ldrNotificationCookie = nullptr;
static HANDLE g_hWatcherThread = NULL;

// Helper: lowercase wstring
static std::wstring ToLowerW(const std::wstring& s)
{
	std::wstring out(s);
	for (auto& c : out) c = towlower(c);
	return out;
}

// Single handler you asked for: receives module base and module name, checks each dll name you set and applies a patch (example).
static void HandleModule(HMODULE moduleHandle, const std::wstring& baseName, bool loaded)
{
	if (!moduleHandle || baseName.empty())
	{
		return;
	}		

	std::wstring name = ToLowerW(baseName);

	if (name == L"vtkernel.dll")
	{
		dllModule2 = moduleHandle;

		if (loaded)
		{
			std::vector<std::uint8_t*> ResolutionListsScansResult = Memory::PatternScan(dllModule2, "C7 83 A0 01 00 00 80 02 00 00 C7 83 A4 01 00 00 E0 01 00 00 EB 2F 83 F8 01 75 16 C7 83 A0 01 00 00 20 03 00 00 C7 83 A4 01 00 00 58 02 00 00 EB 14 C7 83 A0 01 00 00 00 04 00 00 C7 83 A4 01 00 00 00 03 00 00", "C7 86 A0 01 00 00 80 02 00 00 C7 86 A4 01 00 00 E0 01 00 00 EB 2F 83 FB 01 75 16 C7 86 A0 01 00 00 20 03 00 00 C7 86 A4 01 00 00 58 02 00 00 EB 14 C7 86 A0 01 00 00 00 04 00 00 C7 86 A4 01 00 00 00 03 00 00");
			if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
			{
				spdlog::info("Resolution List 1: Address is vtKernel.dll+{:x}", ResolutionListsScansResult[ResolutionList1Scan] - (std::uint8_t*)dllModule2);

				spdlog::info("Resolution List 2: Address is vtKernel.dll+{:x}", ResolutionListsScansResult[ResolutionList2Scan] - (std::uint8_t*)dllModule2);

				// Resolution List 1
				// 640x480
				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 6, iNewResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 16, iNewResY);

				// 800x600
				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 33, iNewResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 43, iNewResY);

				// 1024x768
				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 55, iNewResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList1Scan] + 65, iNewResY);

				// Resolution List 2
				// 640x480
				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 6, iNewResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 16, iNewResY);

				// 800x600
				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 33, iNewResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 43, iNewResY);

				// 1024x768
				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 55, iNewResX);

				Memory::Write(ResolutionListsScansResult[ResolutionList2Scan] + 65, iNewResY);
			}
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "d9 82 ? ? ? ? 57");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is vtKernel.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

			/*
			Memory::PatchBytes(CameraFOVInstructionScanResult, "\x90\x90\x90\x90\x90\x90", 6);

			CameraFOVInstructionHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = *reinterpret_cast<float*>(ctx.edx + 0xE0);

				if (fCurrentCameraFOV == 0.6899999976f)
				{
					fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale) * fFOVFactor;
				}
				else
				{
					fNewCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, fAspectRatioScale);
				}

				FPU::FLD(fNewCameraFOV);
			});
			*/
		}
	}
	else if (name == L"camera.dll")
	{
		dllModule3 = moduleHandle;
		if (loaded)
		{

		}
	}
	else if (name == L"cochetorrente.dll")
	{
		dllModule4 = moduleHandle;
		if (loaded)
		{
			/*
			std::vector<std::uint8_t*> HUDInstructionsScans1Result = Memory::PatternScan(dllModule4, "68 58 02 00 00 68 20 03 00 00 8D 8C 24 84 00 00 00 51 8D 54 24", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 88 00 00 00 51 8D 94", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 9C 00 00 00 51 8D 94 24 A8 00 00 00 52",
				"68 58 02 00 00 68 20 03 00 00 8D 94 24 88 00 00 00 52 8D 84 24 94 00 00 00 50 68 12", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 9C 00 00 00 51 8D 94 24 A8", "8B C7 BD 26 02 00 00 8D B8", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 88 00 00 00 51 8D 94 24 94 00 00 00 52 68 12 02 00 00 55 FF D6 68 58 02 00 00 68 20 03 00 00", "68 0A D7 A3 3F 50 FF 15 40 B1 01 10 83 C4");
			if (Memory::AreAllSignaturesValid(HUDInstructionsScans1Result) == true)
			{

			}
			*/
		}
	}
	else if (name == L"comercial.dll")
	{
		dllModule5 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"contadordetonador.dll")
	{
		dllModule6 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"contadordinero.dll")
	{
		dllModule7 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"contadormunicion.dll")
	{
		dllModule8 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"contadorsalud.dll")
	{
		dllModule9 = moduleHandle;

		if (loaded)
		{

		}
	}	
	else if (name == L"contadortiempo.dll")
	{
		dllModule10 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"fase_cuco.dll")
	{
		dllModule11 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"fase_spinelli.dll")
	{
		dllModule12 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"madrid_f03.dll")
	{
		dllModule13 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"madrid_f04.dll")
	{
		dllModule14 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"madrid_franco.dll")
	{
		dllModule15 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"madrid_kio.dll")
	{
		dllModule16 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"main.dll")
	{
		dllModule17 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"marbella_chalets.dll")
	{
		dllModule18 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"marbella_f01.dll")
	{
		dllModule19 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"marbella_f02.dll")
	{
		dllModule20 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"marbella_f03.dll")
	{
		dllModule21 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"marbella_f04.dll")
	{
		dllModule22 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"marbella_malibu.dll")
		{
		dllModule23 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"radar.dll")
	{
		dllModule24 = moduleHandle;

		if (loaded)
		{

		}
	}
	else if (name == L"script.dll")
	{
		dllModule25 = moduleHandle;

		if (loaded)
		{

		}
	}
}

void WidescreenFix()
{
	if (bFixActive == true && eGameType == Game::TORRENTE)
	{
		fNewAspectRatio = static_cast<float>(iNewResX) / static_cast<float>(iNewResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;
	}
}

static VOID NTAPI LdrNotificationCallback(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA_SIMPLE NotificationData, PVOID Context)
{
	if (!NotificationData)
		return;

	if (NotificationData->BaseDllName && NotificationData->BaseDllName->Buffer)
	{
		std::wstring baseName(NotificationData->BaseDllName->Buffer, NotificationData->BaseDllName->Length / sizeof(wchar_t));
		HMODULE moduleBase = reinterpret_cast<HMODULE>(NotificationData->DllBase);

		if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
		{
			spdlog::info("DLL loaded: {}", std::string(baseName.begin(), baseName.end()));
			HandleModule(moduleBase, baseName, true);
		}
		else if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_UNLOADED)
		{
			spdlog::info("DLL unloaded: {}", std::string(baseName.begin(), baseName.end()));
			HandleModule(moduleBase, baseName, false);
		}
	}
}

static DWORD WINAPI DllWatcherThread(LPVOID lpParameter)
{
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (!ntdll)
    {
        spdlog::warn("DllWatcherThread: ntdll not found.");
        return 1;
    }

    typedef LONG NTSTATUS;

    using LdrRegisterFn = NTSTATUS(NTAPI*)(ULONG, PLDR_DLL_NOTIFICATION_FUNCTION, PVOID, PLDR_DLL_NOTIFICATION_COOKIE*);
    using LdrUnregisterFn = NTSTATUS(NTAPI*)(PLDR_DLL_NOTIFICATION_COOKIE);

    auto pRegister = reinterpret_cast<LdrRegisterFn>(GetProcAddress(ntdll, "LdrRegisterDllNotification"));
    auto pUnregister = reinterpret_cast<LdrUnregisterFn>(GetProcAddress(ntdll, "LdrUnregisterDllNotification"));
	if (!pRegister || !pUnregister)
	{
		spdlog::warn("DllWatcherThread: LdrRegisterDllNotification / LdrUnregisterDllNotification not available on this OS.");
		return 1;
	}

	NTSTATUS status = pRegister(0, reinterpret_cast<PLDR_DLL_NOTIFICATION_FUNCTION>(LdrNotificationCallback), nullptr, &g_ldrNotificationCookie);
	if (status != 0 || !g_ldrNotificationCookie)
	{
		spdlog::warn("DllWatcherThread: registration failed (status 0x{:x}).", (unsigned)status);
		return 1;
	}

	spdlog::info("DllWatcherThread: registered dll notification.");

	HMODULE hMods[1024];
	DWORD cbNeeded = 0;
	if (EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded))
	{
		size_t count = cbNeeded / sizeof(HMODULE);
		for (size_t i = 0; i < count; ++i)
		{
			WCHAR name[MAX_PATH];
			if (GetModuleBaseNameW(GetCurrentProcess(), hMods[i], name, MAX_PATH))
			{
				std::wstring baseName(name);
				HandleModule(hMods[i], baseName, true);
			}
		}
	}

	Sleep(INFINITE);

	pUnregister(g_ldrNotificationCookie);
	g_ldrNotificationCookie = nullptr;
	return 0;
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

		g_hWatcherThread = CreateThread(NULL, 0, DllWatcherThread, NULL, 0, NULL);
		if (g_hWatcherThread)
		{
			SetThreadPriority(g_hWatcherThread, THREAD_PRIORITY_NORMAL);
			CloseHandle(g_hWatcherThread); // keep thread running, don't leak handle
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