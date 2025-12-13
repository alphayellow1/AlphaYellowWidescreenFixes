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
int iNewResX2;
int iNewCocheTorrenteHUDValue5;
int iNewCocheTorrenteHUDValue6;
float fNewCocheTorrenteHUDFOV;
int iNewComercialHUDValue2;
int iNewContadorDetonadorHUDValue2;
int iNewContadorDetonadorHUDValue4;
int iNewContadorDineroHUDValue2;
int iNewContadorDineroHUDValue5;
int iNewContadorTiempoHUDValue2;
int iNewContadorTiempoHUDValue4;
int iNewFaseCucoHUDValue3;
int iNewFaseCucoHUDValue6;
int iNewFaseCucoHUDValue9;
int iNewFaseCucoHUDValue12;
int iNewMadrid_F03HUDValue3;
int iNewMadrid_F03HUDValue5;
int iNewMadrid_F04HUDValue4;
int iNewMadrid_FrancoHUDValue4;
int iNewMadrid_FrancoHUDValue7;
int iNewMadrid_FrancoHUDValue10;
int iNewMarbella_ChaletsHUDValue3;
int iNewMarbella_ChaletsHUDValue6;
int iNewMarbella_F02HUDValue3;
int iNewMarbella_F02HUDValue6;

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

enum CocheTorrenteHUDInstructionsIndices
{
	CocheTorrenteHUD1Scan,
	CocheTorrenteHUD2Scan,
	CocheTorrenteHUD3Scan,
	CocheTorrenteHUD4Scan,
	CocheTorrenteHUD5Scan,
	CocheTorrenteHUD6Scan,
	CocheTorrenteHUD7Scan,
	CocheTorrenteHUD8Scan
};

enum ComercialHUDInstructionsIndices
{
	ComercialHUD1Scan,
	ComercialHUD2Scan,
	ComercialHUD3Scan,
	ComercialHUD4Scan,
	ComercialHUD5Scan,
	ComercialHUD6Scan
};

enum ContadorDetonadorHUDInstructionsIndices
{
	ContadorDetonadorHUD1Scan,
	ContadorDetonadorHUD2Scan,
	ContadorDetonadorHUD3Scan,
	ContadorDetonadorHUD4Scan,
	ContadorDetonadorHUD5Scan,
	ContadorDetonadorHUD6Scan
};

enum ContadorDineroHUDInstructionsIndices
{
	ContadorDineroHUD1Scan,
	ContadorDineroHUD2Scan,
	ContadorDineroHUD3Scan,
	ContadorDineroHUD4Scan,
	ContadorDineroHUD5Scan,
	ContadorDineroHUD6Scan
};

enum ContadorMunicionHUDInstructionsIndices
{
	ContadorMunicionHUD1Scan,
	ContadorMunicionHUD2Scan,
	ContadorMunicionHUD3Scan,
	ContadorMunicionHUD4Scan
};

enum ContadorSaludHUDInstructionsIndices
{
	ContadorSaludHUD1Scan,
	ContadorSaludHUD2Scan,
	ContadorSaludHUD3Scan,
	ContadorSaludHUD4Scan,
	ContadorSaludHUD5Scan,
	ContadorSaludHUD6Scan,
	ContadorSaludHUD7Scan,
	ContadorSaludHUD8Scan
};

enum ContadorTiempoHUDInstructionsIndices
{
	ContadorTiempoHUD1Scan,
	ContadorTiempoHUD2Scan,
	ContadorTiempoHUD3Scan,
	ContadorTiempoHUD4Scan,
	ContadorTiempoHUD5Scan,
	ContadorTiempoHUD6Scan
};

enum FaseCucoHUDInstructionsIndices
{
	FaseCucoHUD1Scan,
	FaseCucoHUD2Scan,
	FaseCucoHUD3Scan,
	FaseCucoHUD4Scan,
	FaseCucoHUD5Scan,
	FaseCucoHUD6Scan,
	FaseCucoHUD7Scan,
	FaseCucoHUD8Scan,
	FaseCucoHUD9Scan,
	FaseCucoHUD10Scan,
	FaseCucoHUD11Scan,
	FaseCucoHUD12Scan,
	FaseCucoHUD13Scan,
	FaseCucoHUD14Scan,
	FaseCucoHUD15Scan
};

enum Madrid_F03HUDInstructionsIndices
{
	Madrid_F03HUD1Scan,
	Madrid_F03HUD2Scan,
	Madrid_F03HUD3Scan,
	Madrid_F03HUD4Scan,
	Madrid_F03HUD5Scan,
	Madrid_F03HUD6Scan
};

enum Madrid_F04HUDInstructionsIndices
{
	Madrid_F04HUD1Scan,
	Madrid_F04HUD2Scan,
	Madrid_F04HUD3Scan,
	Madrid_F04HUD4Scan
};

enum MadridFrancoHUDInstructionsIndices
{
	Madrid_FrancoHUD1Scan,
	Madrid_FrancoHUD2Scan,
	Madrid_FrancoHUD3Scan,
	Madrid_FrancoHUD4Scan,
	Madrid_FrancoHUD5Scan,
	Madrid_FrancoHUD6Scan,
	Madrid_FrancoHUD7Scan,
	Madrid_FrancoHUD8Scan,
	Madrid_FrancoHUD9Scan,
	Madrid_FrancoHUD10Scan,
	Madrid_FrancoHUD11Scan
};

enum MainHUDInstructionsIndices
{
	MainHUD1Scan,
	MainHUD2Scan,
	MainHUD3Scan,
	MainHUD4Scan
};

enum Marbella_ChalettsHUDInstructionsIndices
{
	Marbella_ChalettsHUD1Scan,
	Marbella_ChalettsHUD2Scan,
	Marbella_ChalettsHUD3Scan,
	Marbella_ChalettsHUD4Scan,
	Marbella_ChalettsHUD5Scan,
	Marbella_ChalettsHUD6Scan,
	Marbella_ChalettsHUD7Scan
};

enum Marbella_F02HUDInstructionsIndices
{
	Marbella_F02HUD1Scan,
	Marbella_F02HUD2Scan,
	Marbella_F02HUD3Scan,
	Marbella_F02HUD4Scan,
	Marbella_F02HUD5Scan,
	Marbella_F02HUD6Scan,
	Marbella_F02HUD7Scan,
	Marbella_F02HUD8Scan
};

enum RadarHUDInstructionsIndices
{
	RadarHUD1Scan,
	RadarHUD2Scan,
	RadarHUD3Scan,
	RadarHUD4Scan,
	RadarHUD5Scan,
	RadarHUD6Scan
};

enum ScriptHUDInstructionsIndices
{
	ScriptHUD1Scan,
	ScriptHUD2Scan,
	ScriptHUD3Scan,
	ScriptHUD4Scan,
	ScriptHUD5Scan,
	ScriptHUD6Scan
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

static void HandleModule(HMODULE moduleHandle, const std::wstring& baseName, bool loaded)
{
	if (!moduleHandle || baseName.empty())
	{
		return;
	}		

	std::wstring name = ToLowerW(baseName);

	if (bFixActive == true && eGameType == Game::TORRENTE)
	{
		if (name == L"vtkernel.dll")
		{
			dllModule2 = moduleHandle;

			if (loaded)
			{
				// Located in vtEngine::vtEngine and vtEngine::SetResolution
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

			// Located in vtKernel.dll.vtCamera::ComputeProjectionMatrix
			std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(dllModule2, "D9 82 ?? ?? ?? ?? 57");
			if (CameraFOVInstructionScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is vtKernel.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)dllModule2);

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
			}
		}
		else if (name == L"cochetorrente.dll")
		{
			dllModule4 = moduleHandle;
			if (loaded)
			{
				std::vector<std::uint8_t*> CocheTorrenteHUDInstructionsScansResult = Memory::PatternScan(dllModule4, "68 58 02 00 00 68 20 03 00 00 8D 8C 24 84 00 00 00 51 8D 54 24", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 88 00 00 00 51 8D 94", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 9C 00 00 00 51 8D 94 24 A8 00 00 00 52",
					"68 58 02 00 00 68 20 03 00 00 8D 94 24 88 00 00 00 52 8D 84 24 94 00 00 00 50 68 12", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 9C 00 00 00 51 8D 94 24 A8", "8B C7 BD 26 02 00 00 8D B8", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 88 00 00 00 51 8D 94 24 94 00 00 00 52 68 12 02 00 00 55 FF D6 68 58 02 00 00 68 20 03 00 00", "68 0A D7 A3 3F 50 FF 15 40 B1 01 10 83 C4");
				if (Memory::AreAllSignaturesValid(CocheTorrenteHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD1Scan] - (std::uint8_t*)dllModule4);

					spdlog::info("HUD Instruction 2: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD2Scan] - (std::uint8_t*)dllModule4);

					spdlog::info("HUD Instruction 3: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD3Scan] - (std::uint8_t*)dllModule4);

					spdlog::info("HUD Instruction 4: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD4Scan] - (std::uint8_t*)dllModule4);

					spdlog::info("HUD Instruction 5: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD5Scan] - (std::uint8_t*)dllModule4);

					spdlog::info("HUD Instruction 6: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD6Scan] - (std::uint8_t*)dllModule4);

					spdlog::info("HUD Instruction 7: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD7Scan] - (std::uint8_t*)dllModule4);

					spdlog::info("HUD Instruction 8: Address is CocheTorrente.dll+{:x}", CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD8Scan] - (std::uint8_t*)dllModule4);

					// HUD Instructions 1
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					iNewCocheTorrenteHUDValue6 = (int)(std::round(598.5f * fNewAspectRatio - 248.0f));
					
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD6Scan] + 2, iNewCocheTorrenteHUDValue6);

					// HUD Instructions 7
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD7Scan] + 6, iNewResX2);

					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD7Scan] + 40, iNewResX2);

					// HUD Instructions 8
					fNewCocheTorrenteHUDFOV = Maths::CalculateNewFOV_RadBased(1.27999997138977f, fAspectRatioScale);
					
					Memory::Write(CocheTorrenteHUDInstructionsScansResult[CocheTorrenteHUD8Scan] + 1, fNewCocheTorrenteHUDFOV);
				}
			}
		}
		else if (name == L"comercial.dll")
		{
			dllModule5 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> ComercialHUDInstructionsScansResult = Memory::PatternScan(dllModule5, "68 58 02 00 00 68 20 03 00 00 8D 54 24 14 52 8D 44 24 1C 50 68 3D 02 00 00", "68 02 03 00 00 FF D6 B9 00 A2 00 10 83 C4 18 85 C9 75 0A 89 6C 24 2C 89 6C 24 28 EB 3F 83 C9 FF", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 14 51 8D 54 24 1C 52 6A 12 6A 12 FF D6 8B 44 24 24 8B 4C 24 28", "68 58 02 00 00 68 20 03 00 00 8D 54 24 50 52 8D 44 24 58 50 68 2F 02 00 00 68 E2 02 00 00",
				"68 E2 02 00 00 FF D6 83 C4 54 68 D8 A1 00 10 8D 4C 24 2C FF 15 ?? ?? ?? ??", "68 58 02 00 00 68 20 03 00 00 8D 84 24 88 00 00 00 50 8D 8C 24 88 00 00 00 51 6A 20 C7 86 48 03 00 00 01 00 00 00 6A 20 C6 86 CC 00 00 00 01 FF 15 ?? ?? ?? ??");
				if (Memory::AreAllSignaturesValid(ComercialHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is Comercial.dll+{:x}", ComercialHUDInstructionsScansResult[ComercialHUD1Scan] - (std::uint8_t*)dllModule5);

					spdlog::info("HUD Instruction 2: Address is Comercial.dll+{:x}", ComercialHUDInstructionsScansResult[ComercialHUD2Scan] - (std::uint8_t*)dllModule5);

					spdlog::info("HUD Instruction 3: Address is Comercial.dll+{:x}", ComercialHUDInstructionsScansResult[ComercialHUD3Scan] - (std::uint8_t*)dllModule5);

					spdlog::info("HUD Instruction 4: Address is Comercial.dll+{:x}", ComercialHUDInstructionsScansResult[ComercialHUD4Scan] - (std::uint8_t*)dllModule5);

					spdlog::info("HUD Instruction 5: Address is Comercial.dll+{:x}", ComercialHUDInstructionsScansResult[ComercialHUD5Scan] - (std::uint8_t*)dllModule5);

					spdlog::info("HUD Instruction 6: Address is Comercial.dll+{:x}", ComercialHUDInstructionsScansResult[ComercialHUD6Scan] - (std::uint8_t*)dllModule5);

					// HUD Instructions 1
					Memory::Write(ComercialHUDInstructionsScansResult[ComercialHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					iNewComercialHUDValue2 = (int)(std::round(600.0f * fNewAspectRatio - 30.0f));

					Memory::Write(ComercialHUDInstructionsScansResult[ComercialHUD2Scan] + 1, iNewComercialHUDValue2);

					// HUD Instructions 3
					Memory::Write(ComercialHUDInstructionsScansResult[ComercialHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					Memory::Write(ComercialHUDInstructionsScansResult[ComercialHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					iNewCocheTorrenteHUDValue5 = (int)(std::round(600.0f * fNewAspectRatio - 62.0f));

					Memory::Write(ComercialHUDInstructionsScansResult[ComercialHUD5Scan] + 1, iNewCocheTorrenteHUDValue5);

					// HUD Instructions 6
					Memory::Write(ComercialHUDInstructionsScansResult[ComercialHUD6Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"contadordetonador.dll")
		{
			dllModule6 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> ContadorDetonadorHUDInstructionsScansResult = Memory::PatternScan(dllModule6, "68 58 02 00 00 68 20 03 00 00 8D 44 24 44 50 8D 4C 24 44 51 68 2F 02 00 00 68 E3", "68 E3 02 00 00 89 5C 24 44 C7 06 80 20 00 10 FF 15 ?? ?? ?? ??", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 44 51 8D 54 24 44 52 68 3E 02 00 00 68 00 03 00 00", "68 00 03 00 00 FF 15 ?? ?? ?? ?? B8 F0 30 00 10 83 C4 18 85 C0", "68 58 02 00 00 68 20 03 00 00 8D 44 24 54 50 8D 4C 24 54 51 6A 27 6A 34 FF D5 8B 54 24 64 8B", "68 58 02 00 00 68 20 03 00 00 8D 54 24 78 52 8D 44 24 78 50 6A 11 6A 11 FF D5 8B 8C 24 88");
				if (Memory::AreAllSignaturesValid(ContadorDetonadorHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is ContadorDetonador.dll+{:x}", ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD1Scan] - (std::uint8_t*)dllModule6);

					spdlog::info("HUD Instruction 2: Address is ContadorDetonador.dll+{:x}", ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD2Scan] - (std::uint8_t*)dllModule6);

					spdlog::info("HUD Instruction 3: Address is ContadorDetonador.dll+{:x}", ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD3Scan] - (std::uint8_t*)dllModule6);

					spdlog::info("HUD Instruction 4: Address is ContadorDetonador.dll+{:x}", ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD4Scan] - (std::uint8_t*)dllModule6);

					spdlog::info("HUD Instruction 5: Address is ContadorDetonador.dll+{:x}", ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD5Scan] - (std::uint8_t*)dllModule6);

					spdlog::info("HUD Instruction 6: Address is ContadorDetonador.dll+{:x}", ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD6Scan] - (std::uint8_t*)dllModule6);

					// HUD Instructions 1
					Memory::Write(ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					iNewContadorDetonadorHUDValue2 = (int)(std::round(600.0f * fNewAspectRatio - 61.0f));

					Memory::Write(ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD2Scan] + 1, iNewContadorDetonadorHUDValue2);

					// HUD Instructions 3
					Memory::Write(ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					iNewContadorDetonadorHUDValue4 = (int)(std::round(600.0f * fNewAspectRatio - 32.0f));

					Memory::Write(ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD4Scan] + 1, iNewContadorDetonadorHUDValue4);

					// HUD Instructions 5
					Memory::Write(ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					Memory::Write(ContadorDetonadorHUDInstructionsScansResult[ContadorDetonadorHUD6Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"contadordinero.dll")
		{
			dllModule7 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> ContadorDineroHUDInstructionsScansResult = Memory::PatternScan(dllModule7, "68 58 02 00 00 68 20 03 00 00 8D 44 24 44 50 8D 4C 24 44 51 68 2E 02 00 00 68 7A 02 00", "68 7A 02 00 00 89 5C 24 44 C7 06 78 20 00 10 FF D5 BA EC 30 00 10 83 C4 18 85 D2 75 0A", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 44 51 8D 54 24 44 52 6A 26 68 9C 00 00 00 FF", "68 58 02 00 00 68 20 03 00 00 8D 54 24 20 52 8D 44 24 28 50 68 3E 02 00 00 68 9E 02 00 00 FF D5 B9 EC", "68 9E 02 00 00 FF D5 B9 EC 30 00 10 83 C4 20 85 C9 75 0A 89 5C 24 24 89 5C 24 20 EB 3F 83 C9 FF 33", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 18 51 8D 54 24 20 52 6A 11 6A 11 89 5C 24 3C FF D5 8B 44 24 28 8B 4C 24 2C");
				if (Memory::AreAllSignaturesValid(ContadorDineroHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is ContadorDinero.dll+{:x}", ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD1Scan] - (std::uint8_t*)dllModule7);

					spdlog::info("HUD Instruction 2: Address is ContadorDinero.dll+{:x}", ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD2Scan] - (std::uint8_t*)dllModule7);

					spdlog::info("HUD Instruction 3: Address is ContadorDinero.dll+{:x}", ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD3Scan] - (std::uint8_t*)dllModule7);

					spdlog::info("HUD Instruction 4: Address is ContadorDinero.dll+{:x}", ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD4Scan] - (std::uint8_t*)dllModule7);

					spdlog::info("HUD Instruction 5: Address is ContadorDinero.dll+{:x}", ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD5Scan] - (std::uint8_t*)dllModule7);

					spdlog::info("HUD Instruction 6: Address is ContadorDinero.dll+{:x}", ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD6Scan] - (std::uint8_t*)dllModule7);

					// HUD Instructions 1
					Memory::Write(ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					iNewContadorDineroHUDValue2 = (int)(std::round(600.0f * fNewAspectRatio - 166.0f));

					Memory::Write(ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD2Scan] + 1, iNewContadorDineroHUDValue2);

					// HUD Instructions 3
					Memory::Write(ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					Memory::Write(ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					iNewContadorDineroHUDValue5 = (int)(std::round(600.0f * fNewAspectRatio - 130.0f));

					Memory::Write(ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD5Scan] + 1, iNewContadorDineroHUDValue5);

					// HUD Instructions 6
					Memory::Write(ContadorDineroHUDInstructionsScansResult[ContadorDineroHUD6Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"contadormunicion.dll")
		{
			dllModule8 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> ContadorMunicionHUDInstructionsScansResult = Memory::PatternScan(dllModule8, "68 58 02 00 00 68 20 03 00 00 8D 44 24 40 50 8D 4C 24 40 51 68 29 02 00 00 6A 5F 89 5C 24", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 44 51 8D 54 24 44 52 68 3E 02 00 00 68 87 00 00 00 FF 15 ?? ?? ?? ??", "68 58 02 00 00 68 20 03 00 00 8D 54 24 40 52 8D 44 24 40 50 6A 29 68 80 00 00 00 FF D3 8B 4C 24 50 8B 54 24 4C 8B", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 64 51 8D 54 24 64 52 6A 11 6A 11 FF D3 8B 44 24 74 8B 4C 24 70 8B 56 28 50");
				if (Memory::AreAllSignaturesValid(ContadorMunicionHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is ContadorMunicion.dll+{:x}", ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD1Scan] - (std::uint8_t*)dllModule8);

					spdlog::info("HUD Instruction 2: Address is ContadorMunicion.dll+{:x}", ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD2Scan] - (std::uint8_t*)dllModule8);

					spdlog::info("HUD Instruction 3: Address is ContadorMunicion.dll+{:x}", ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD3Scan] - (std::uint8_t*)dllModule8);

					spdlog::info("HUD Instruction 4: Address is ContadorMunicion.dll+{:x}", ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD4Scan] - (std::uint8_t*)dllModule8);

					// HUD Instructions 1
					Memory::Write(ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					Memory::Write(ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					Memory::Write(ContadorMunicionHUDInstructionsScansResult[ContadorMunicionHUD4Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"contadorsalud.dll")
		{
			dllModule9 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> ContadorSaludHUDInstructionsScansResult = Memory::PatternScan(dllModule9, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 2B C6", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 56");
				if (Memory::AreAllSignaturesValid(ContadorSaludHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD1Scan] - (std::uint8_t*)dllModule9);

					spdlog::info("HUD Instruction 2: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD2Scan] - (std::uint8_t*)dllModule9);

					spdlog::info("HUD Instruction 3: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD3Scan] - (std::uint8_t*)dllModule9);

					spdlog::info("HUD Instruction 4: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD4Scan] - (std::uint8_t*)dllModule9);

					spdlog::info("HUD Instruction 5: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD5Scan] - (std::uint8_t*)dllModule9);

					spdlog::info("HUD Instruction 6: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD6Scan] - (std::uint8_t*)dllModule9);

					spdlog::info("HUD Instruction 7: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD7Scan] - (std::uint8_t*)dllModule9);

					spdlog::info("HUD Instruction 8: Address is ContadorSalud.dll+{:x}", ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD8Scan] - (std::uint8_t*)dllModule9);

					// HUD Instructions 1
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD6Scan] + 6, iNewResX2);

					// HUD Instructions 7
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD7Scan] + 1, iNewResX2);

					// HUD Instructions 8
					Memory::Write(ContadorSaludHUDInstructionsScansResult[ContadorSaludHUD8Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"contadortiempo.dll")
		{
			dllModule10 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> ContadorTiempoHUDInstructionsScansResult = Memory::PatternScan(dllModule10, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 68 ?? ?? ?? ?? FF D5 B9", "68 ?? ?? ?? ?? FF D5 B9", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24", "68 ?? ?? ?? ?? FF D5 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 6A ?? 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 6A ?? 6A");
				if (Memory::AreAllSignaturesValid(ContadorTiempoHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is ContadorTiempo.dll+{:x}", ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD1Scan] - (std::uint8_t*)dllModule10);

					spdlog::info("HUD Instruction 2: Address is ContadorTiempo.dll+{:x}", ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD2Scan] - (std::uint8_t*)dllModule10);

					spdlog::info("HUD Instruction 3: Address is ContadorTiempo.dll+{:x}", ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD3Scan] - (std::uint8_t*)dllModule10);

					spdlog::info("HUD Instruction 4: Address is ContadorTiempo.dll+{:x}", ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD4Scan] - (std::uint8_t*)dllModule10);

					spdlog::info("HUD Instruction 5: Address is ContadorTiempo.dll+{:x}", ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD5Scan] - (std::uint8_t*)dllModule10);

					spdlog::info("HUD Instruction 6: Address is ContadorTiempo.dll+{:x}", ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD6Scan] - (std::uint8_t*)dllModule10);

					// HUD Instructions 1
					Memory::Write(ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					iNewContadorTiempoHUDValue2 = (int)(std::round(600.0f * fNewAspectRatio - 175.0f));

					Memory::Write(ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD2Scan] + 1, iNewContadorTiempoHUDValue2);

					// HUD Instructions 3
					Memory::Write(ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					iNewContadorTiempoHUDValue4 = (int)(std::round(600.0f * fNewAspectRatio - 178.0f));

					Memory::Write(ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD4Scan] + 1, iNewContadorTiempoHUDValue4);

					// HUD Instructions 5
					Memory::Write(ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					Memory::Write(ContadorTiempoHUDInstructionsScansResult[ContadorTiempoHUD6Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"fase_cuco.dll")
		{
			dllModule11 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> FaseCucoHUDInstructionsScansResult = Memory::PatternScan(dllModule11, "68 58 02 00 00 68 20 03 00 00 8D 94 24 90 00 00 00 52 8D 84 24 90 00 00 00 50 6A 20 6A 20 FF 15 20 91 00 10", "68 58 02 00 00 68 20 03 00 00 8D 84 24 90 00 00 00 50 8D 8C 24 90 00 00 00 51 68 3F 02 00 00 68 BF 02", "68 BF 02 00 00 FF D7 68 58 02 00 00 68 20 03 00 00 8D 94 24 B0 00 00 00 52 8D 84 24 B0", "68 BF 02 00 00 FF D7 68 58 02 00 00 68 20 03 00 00 8D 94 24 B0 00 00 00 52 8D 84 24 B0", "68 58 02 00 00 68 20 03 00 00 8D 84 24 88 00 00 00 50 8D 8C 24 88 00 00 00 51 68 30 02 00 00 68 A1 02", "68 A1 02 00 00 FF D7 68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84 24 A8 00 00 00 50 6A 22 6A", 
					"68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84 24 A8 00 00 00 50 6A 22 6A 76", "68 58 02 00 00 68 20 03 00 00 8D 84 24 88 00 00 00 50 8D 8C 24 88 00 00 00 51 68 30 02 00 00 68 AD 02 00 00 FF D7 68 58 02 00 00 68", "68 AD 02 00 00 FF D7 68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84 24", "68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84 24 A8 00 00 00 50 6A 22 6A 6B FF D7 83 C4 30 68 84", "68 58 02 00 00 68 20 03 00 00 8D 84 24 88 00 00 00 50 8D 8C 24 88 00 00 00 51 68 3F 02 00 00 68 E4 02", 
					"68 E4 02 00 00 FF D7 68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84 24 A8 00 00 00 50 6A", "68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84 24 A8 00 00 00 50 6A 10 6A 10 FF", "68 58 02 00 00 68 20 03 00 00 8D 54 24 20 52 8D 44 24 28 50 6A 0B 6A 56 89 4C 24 28 FF 15 20 91 00 10", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 24 51 8D 54 24 2C 52 6A 0B 6A 56 89 44 24 28 FF 15 20 91 00 10 D9 44 24");
				if (Memory::AreAllSignaturesValid(FaseCucoHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD1Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 2: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD2Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 3: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD3Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 4: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD4Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 5: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD5Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 6: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD6Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 7: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD7Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 8: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD8Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 9: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD9Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 10: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD10Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 11: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD11Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 12: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD12Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 13: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD13Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 14: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD14Scan] - (std::uint8_t*)dllModule11);

					spdlog::info("HUD Instruction 15: Address is fase_cuco.dll+{:x}", FaseCucoHUDInstructionsScansResult[FaseCucoHUD15Scan] - (std::uint8_t*)dllModule11);

					// HUD Instructions 1
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					iNewFaseCucoHUDValue3 = (int)(std::round(600.0f * fNewAspectRatio - 97.0f));

					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD3Scan] + 1, iNewFaseCucoHUDValue3);

					// HUD Instructions 4
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					iNewFaseCucoHUDValue6 = (int)(std::round(600.0f * fNewAspectRatio - 127.0f));

					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD6Scan] + 1, iNewFaseCucoHUDValue6);

					// HUD Instructions 7
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD7Scan] + 6, iNewResX2);

					// HUD Instructions 8
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD8Scan] + 6, iNewResX2);

					// HUD Instructions 9
					iNewFaseCucoHUDValue9 = (int)(std::round(600.0f * fNewAspectRatio - 115.0f));

					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD9Scan] + 1, iNewFaseCucoHUDValue9);

					// HUD Instructions 10
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD10Scan] + 6, iNewResX2);

					// HUD Instructions 11
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD11Scan] + 6, iNewResX2);

					// HUD Instructions 12
					iNewFaseCucoHUDValue12 = (int)(std::round(600.0f * fNewAspectRatio - 60.0f));

					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD12Scan] + 1, iNewFaseCucoHUDValue12);

					// HUD Instructions 13
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD13Scan] + 6, iNewResX2);

					// HUD Instructions 14
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD14Scan] + 6, iNewResX2);

					// HUD Instructions 15
					Memory::Write(FaseCucoHUDInstructionsScansResult[FaseCucoHUD15Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"fase_spinelli.dll")
		{
			dllModule12 = moduleHandle;

			if (loaded)
			{
				std::uint8_t* FaseSpinelliHUDInstructionScanResult = Memory::PatternScan(dllModule12, "68 58 02 00 00 68 20 03 00 00 8D 8C 24 88 00 00 00 51 8D 94 24 88 00 00 00 52 6A 20 6A 20 C6 86 CC 00 00 00 01 FF 15 ?? ?? ?? ??");
				if (FaseSpinelliHUDInstructionScanResult)
				{
					spdlog::info("HUD Instruction: Address is FASE_SPINELLI.dll+{:x}", FaseSpinelliHUDInstructionScanResult - (std::uint8_t*)dllModule12);

					Memory::Write(FaseSpinelliHUDInstructionScanResult + 6, iNewResX2);
				}
				else
				{
					spdlog::info("Cannot locate the HUD instruction memory address (FASE_SPINELLI.dll).");
					return;
				}
			}
		}
		else if (name == L"madrid_f03.dll")
		{
			dllModule13 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> Madrid_F03HUDInstructionsScansResult = Memory::PatternScan(dllModule13, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 6A", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 68",
				"68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 68", "68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24");
				if (Memory::AreAllSignaturesValid(Madrid_F03HUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is Madrid_F03.dll+{:x}", Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD1Scan] - (std::uint8_t*)dllModule13);

					spdlog::info("HUD Instruction 2: Address is Madrid_F03.dll+{:x}", Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD2Scan] - (std::uint8_t*)dllModule13);

					spdlog::info("HUD Instruction 3: Address is Madrid_F03.dll+{:x}", Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD3Scan] - (std::uint8_t*)dllModule13);

					spdlog::info("HUD Instruction 4: Address is Madrid_F03.dll+{:x}", Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD4Scan] - (std::uint8_t*)dllModule13);

					spdlog::info("HUD Instruction 5: Address is Madrid_F03.dll+{:x}", Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD5Scan] - (std::uint8_t*)dllModule13);

					spdlog::info("HUD Instruction 6: Address is Madrid_F03.dll+{:x}", Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD6Scan] - (std::uint8_t*)dllModule13);

					// HUD Instructions 1
					Memory::Write(Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					iNewMadrid_F03HUDValue3 = (int)(std::round(210.0f * fNewAspectRatio - 31.0f));

					Memory::Write(Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD3Scan] + 1, iNewMadrid_F03HUDValue3);

					// HUD Instructions 4
					Memory::Write(Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					iNewMadrid_F03HUDValue5 = (int)(std::round(210.0f * fNewAspectRatio - 35.0f));

					Memory::Write(Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD5Scan] + 1, iNewMadrid_F03HUDValue5);

					// HUD Instructions 6
					Memory::Write(Madrid_F03HUDInstructionsScansResult[Madrid_F03HUD6Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"madrid_f04.dll")
		{
			dllModule14 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> Madrid_F04HUDInstructionsScansResult = Memory::PatternScan(dllModule14, "68 20 03 00 00 C7 44 24 34 00 00 00 00 8B 54 24 34 89", "68 20 03 00 00 8D 44 24 24 50 8D 4C 24 2C 51 6A 32 6A", "68 58 02 00 00 68 20 03 00 00 8D 94 24 B8 00 00 00 52 8D 84 24", "68 F4 02 00 00 FF D7 83 C4 3C 68 78 E3 00 10 8D 4C 24 28 FF 15 AC C0");
				if (Memory::AreAllSignaturesValid(Madrid_F04HUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is Madrid_F04.dll+{:x}", Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD1Scan] - (std::uint8_t*)dllModule14);

					spdlog::info("HUD Instruction 2: Address is Madrid_F04.dll+{:x}", Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD2Scan] - (std::uint8_t*)dllModule14);

					spdlog::info("HUD Instruction 3: Address is Madrid_F04.dll+{:x}", Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD3Scan] - (std::uint8_t*)dllModule14);

					spdlog::info("HUD Instruction 4: Address is Madrid_F04.dll+{:x}", Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD4Scan] - (std::uint8_t*)dllModule14);

					// HUD Instructions 1
					Memory::Write(Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD1Scan] + 1, iNewResX2);

					// HUD Instructions 2
					Memory::Write(Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD2Scan] + 1, iNewResX2);

					// HUD Instructions 3
					Memory::Write(Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					iNewMadrid_F04HUDValue4 = (int)(std::round(600.0f * fNewAspectRatio - 44.0f));

					Memory::Write(Madrid_F04HUDInstructionsScansResult[Madrid_F04HUD4Scan] + 1, iNewMadrid_F04HUDValue4);
				}
			}
		}
		else if (name == L"madrid_franco.dll")
		{
			dllModule15 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> Madrid_FrancoInstructionsScansResult = Memory::PatternScan(dllModule15, "68 58 02 00 00 68 20 03 00 00 8D 8C 24 98 00 00 00 51 8D 94 24 94 00 00 00 52", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 3C 51 8B 8E F0 01 00 00 8D 54 24 38 52 B8", "68 58 02 00 00 68 20 03 00 00 8D 54 24 1C 52 8D 44 24 1C 50 68 3D 02 00 00 68 7F", 
					"68 7F 02 00 00 FF D6 B9 08 D9 00 10 83 C4 18 85 C9 75 0A 89 5C 24", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 6C 51 8D 54 24 24 52 6A 13 6A 26", "68 58 02 00 00 68 20 03 00 00 8D 44 24 40 50 8D 4C 24 40 51 68 43 02 00", "68 AF 02 00 00 FF D6 BA 08 D9 00 10 83 C4 3C 85 D2 75 0A 89 5C 24 2C 89 5C 24 28", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 6C 51 8D 54 24 24 52 6A 0E 6A 66 FF", "68 58 02 00 00 68 20 03 00 00 8D 44 24 40 50 8D 4C 24 40 51 68 45", "68 B1 02 00 00 FF D6 68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 44 24", "68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 44 24 60 50");
				if (Memory::AreAllSignaturesValid(Madrid_FrancoInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD1Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 2: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD2Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 3: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD3Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 4: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD4Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 5: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD5Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 6: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD6Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 7: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD7Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 8: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD8Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 9: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD9Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 10: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD10Scan] - (std::uint8_t*)dllModule15);

					spdlog::info("HUD Instruction 11: Address is madrid_franco.dll+{:x}", Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD11Scan] - (std::uint8_t*)dllModule15);

					// HUD Instructions 1
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					iNewMadrid_FrancoHUDValue4 = (int)(std::round(600.0f * fNewAspectRatio - 161.0f));

					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD4Scan] + 1, iNewMadrid_FrancoHUDValue4);

					// HUD Instructions 5
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD5Scan] + 1, iNewResX2);

					// HUD Instructions 6
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD6Scan] + 1, iNewResX2);

					// HUD Instructions 7
					iNewMadrid_FrancoHUDValue7 = (int)(std::round(600.0f * fNewAspectRatio - 113.0f));

					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD7Scan] + 1, iNewMadrid_FrancoHUDValue7);

					// HUD Instructions 8
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD8Scan] + 1, iNewResX2);

					// HUD Instructions 9
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD9Scan] + 1, iNewResX2);

					// HUD Instructions 10
					iNewMadrid_FrancoHUDValue10 = (int)(std::round(412.5f * fNewAspectRatio + 139.0f));

					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD10Scan] + 1, iNewMadrid_FrancoHUDValue10);

					// HUD Instructions 11
					Memory::Write(Madrid_FrancoInstructionsScansResult[Madrid_FrancoHUD11Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"madrid_kio.dll")
		{
			dllModule16 = moduleHandle;

			if (loaded)
			{
				std::uint8_t* Madrid_KioHUDInstructionScanResult = Memory::PatternScan(dllModule16, "68 58 02 00 00 68 20 03 00 00 8D 8C 24 94 00 00 00 51 8D 94 24");
				if (Madrid_KioHUDInstructionScanResult)
				{
					spdlog::info("HUD Instruction: Address is madrid_kio.dll+{:x}", Madrid_KioHUDInstructionScanResult - (std::uint8_t*)dllModule16);

					Memory::Write(Madrid_KioHUDInstructionScanResult + 6, iNewResX2);
				}
				else
				{
					spdlog::info("Cannot locate the HUD instruction memory address (madrid_kio.dll).");
					return;
				}
			}
		}
		else if (name == L"main.dll")
		{
			dllModule17 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> MainHUDInstructionsScansResult = Memory::PatternScan(dllModule17, "06 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00", "06 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00 8D 44 24 30 50 8D 4C 24 38 51 68 58 02 00 00 68 20 03 00 00", "00 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00", "00 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00 8D 44 24 30 50 8D 4C 24 38 51 68 58 02 00 00 68 20 03 00 00");
				if (Memory::AreAllSignaturesValid(MainHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is Main.dll+{:x}", MainHUDInstructionsScansResult[MainHUD1Scan] + 18 - (std::uint8_t*)dllModule17);

					spdlog::info("HUD Instruction 2: Address is Main.dll+{:x}", MainHUDInstructionsScansResult[MainHUD2Scan] + 38 - (std::uint8_t*)dllModule17);

					spdlog::info("HUD Instruction 3: Address is Main.dll+{:x}", MainHUDInstructionsScansResult[MainHUD3Scan] + 18 - (std::uint8_t*)dllModule17);

					spdlog::info("HUD Instruction 4: Address is Main.dll+{:x}", MainHUDInstructionsScansResult[MainHUD4Scan] + 38 - (std::uint8_t*)dllModule17);

					// HUD Instructions 1
					Memory::Write(MainHUDInstructionsScansResult[MainHUD1Scan] + 19, iNewResX2);

					// HUD Instructions 2
					Memory::Write(MainHUDInstructionsScansResult[MainHUD2Scan] + 39, iNewResX2);

					// HUD Instructions 3
					Memory::Write(MainHUDInstructionsScansResult[MainHUD3Scan] + 19, iNewResX2);

					// HUD Instructions 4
					Memory::Write(MainHUDInstructionsScansResult[MainHUD4Scan] + 39, iNewResX2);
				}
			}
		}
		else if (name == L"marbella_chalets.dll")
		{
			dllModule18 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> Marbella_ChaletsHUDInstructionsScansResult = Memory::PatternScan(dllModule18, "68 58 02 00 00 68 20 03 00 00 8D 94 24 90 00 00 00 52 8D 84 24 90 00", "68 58 02 00 00 68 20 03 00 00 8D 94 24 88 00 00 00 52 8D 84 24 88 00", "68 EB 02 00 00 FF 15 10 81 00 10 B9 F8 A2 00 10 83 C4 18 85 C9 75 0A 89", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 90 00 00 00 51 8D 94 24 90 00 00 00 52 6A 1E 6A 2C FF D7 8B 84 24",
				"68 58 02 00 00 68 20 03 00 00 8D 8C 24 90 00 00 00 51 8D 94 24 90 00 00 00 52 6A 1E 6A 2C FF D7 8B 84 24", "68 02 03 00 00 FF D7 B8 F8 A2 00 10 83 C4 44 85 C0 75 0A 89 5C 24 24", "68 58 02 00 00 68 20 03 00 00 8D 8C 24 90 00 00 00 51 8D 94 24 90 00 00 00 52 6A 11");
				if (Memory::AreAllSignaturesValid(Marbella_ChaletsHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is marbella_chalets.dll+{:x}", Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD1Scan] - (std::uint8_t*)dllModule18);

					spdlog::info("HUD Instruction 2: Address is marbella_chalets.dll+{:x}", Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD2Scan] - (std::uint8_t*)dllModule18);

					spdlog::info("HUD Instruction 3: Address is marbella_chalets.dll+{:x}", Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD3Scan] - (std::uint8_t*)dllModule18);

					spdlog::info("HUD Instruction 4: Address is marbella_chalets.dll+{:x}", Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD4Scan] - (std::uint8_t*)dllModule18);

					spdlog::info("HUD Instruction 5: Address is marbella_chalets.dll+{:x}", Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD5Scan] - (std::uint8_t*)dllModule18);

					spdlog::info("HUD Instruction 6: Address is marbella_chalets.dll+{:x}", Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD6Scan] - (std::uint8_t*)dllModule18);

					spdlog::info("HUD Instruction 7: Address is marbella_chalets.dll+{:x}", Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD7Scan] - (std::uint8_t*)dllModule18);

					// HUD Instructions 1
					Memory::Write(Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					iNewMarbella_ChaletsHUDValue3 = (int)(std::round(600.0f * fNewAspectRatio - 53.0f));

					Memory::Write(Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD3Scan] + 1, iNewMarbella_ChaletsHUDValue3);

					// HUD Instructions 4
					Memory::Write(Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					Memory::Write(Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					iNewMarbella_ChaletsHUDValue6 = (int)(std::round(600.0f * fNewAspectRatio - 30.0f));

					Memory::Write(Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD6Scan] + 1, iNewMarbella_ChaletsHUDValue6);

					// HUD Instructions 7
					Memory::Write(Marbella_ChaletsHUDInstructionsScansResult[Marbella_ChalettsHUD7Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"marbella_f01.dll")
		{
			dllModule19 = moduleHandle;

			if (loaded)
			{
				std::uint8_t* Marbella_F01HUDInstructionScanResult = Memory::PatternScan(dllModule19, "68 58 02 00 00 68 20 03 00 00 8D 54 24 34 52 8D 44 24 3C 50 6A 20 6A");
				if (Marbella_F01HUDInstructionScanResult)
				{
					spdlog::info("HUD Instruction: Address is Marbella_f01.dll+{:x}", Marbella_F01HUDInstructionScanResult - (std::uint8_t*)dllModule19);

					Memory::Write(Marbella_F01HUDInstructionScanResult + 6, iNewResX2);
				}
				else
				{
					spdlog::info("Cannot locate the HUD instruction memory address (Marbella_f01.dll).");
					return;
				}
			}
		}
		else if (name == L"marbella_f02.dll")
		{
			dllModule20 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> Marbella_F02HUDInstructionsScansResult = Memory::PatternScan(dllModule20, "68 58 02 00 00 68 20 03 00 00 8D 94 24 90 00 00 00 52 8D 84 24 90 00 00 00 50 6A 20", "68 20 03 00 00 8D 84 24 90 00 00 00 8B 3D 80 81 00 10 50 8D 8C 24", "68 B3 02 00 00 FF D7 68 58 02 00 00 68 20 03 00 00 8D 94 24 B0 00 00 00", "68 20 03 00 00 8D 94 24 B0 00 00 00 52 8D 84 24 B0 00 00 00 50 6A 0B 6A 62 FF D7", "68 58 02 00 00 68 20 03 00 00 8D 84 24 88 00 00 00 50 8D 8C 24 88 00 00 00 51", "68 8B 02 00 00 FF D7 68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84", "68 58 02 00 00 68 20 03 00 00 8D 94 24 A8 00 00 00 52 8D 84 24 A8", "68 58 02 00 00 68 20 03 00 00 8D 94 24 AC 00 00 00 52 8D 84 24 A4 00 00 00 50 6A");
				if (Memory::AreAllSignaturesValid(Marbella_F02HUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD1Scan] - (std::uint8_t*)dllModule20);

					spdlog::info("HUD Instruction 2: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD2Scan] - (std::uint8_t*)dllModule20);

					spdlog::info("HUD Instruction 3: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD3Scan] - (std::uint8_t*)dllModule20);

					spdlog::info("HUD Instruction 4: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD4Scan] - (std::uint8_t*)dllModule20);

					spdlog::info("HUD Instruction 5: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD5Scan] - (std::uint8_t*)dllModule20);

					spdlog::info("HUD Instruction 6: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD6Scan] - (std::uint8_t*)dllModule20);

					spdlog::info("HUD Instruction 7: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD7Scan] - (std::uint8_t*)dllModule20);

					spdlog::info("HUD Instruction 8: Address is Marbella_f02.dll+{:x}", Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD8Scan] - (std::uint8_t*)dllModule20);

					// HUD Instructions 1
					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD2Scan] + 1, iNewResX2);

					// HUD Instructions 3
					iNewMarbella_F02HUDValue3 = (int)(std::round(600.0f * fNewAspectRatio - 109.0f));

					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD3Scan] + 1, iNewMarbella_F02HUDValue3);

					// HUD Instructions 4
					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD4Scan] + 1, iNewResX2);

					// HUD Instructions 5
					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					iNewMarbella_F02HUDValue6 = (int)(std::round(600.0f * fNewAspectRatio - 149.0f));

					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD6Scan] + 1, iNewMarbella_F02HUDValue6);

					// HUD Instructions 7
					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD7Scan] + 6, iNewResX2);

					// HUD Instructions 8
					Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD8Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"marbella_f03.dll")
		{
			dllModule21 = moduleHandle;

			if (loaded)
			{
				std::uint8_t* Marbella_F03HUDInstructionScanResult = Memory::PatternScan(dllModule21, "68 58 02 00 00 68 20 03 00 00 8D 54 24 3C 52 8D 44 24 48 50 6A 20");
				if (Marbella_F03HUDInstructionScanResult)
				{
					spdlog::info("HUD Instruction: Address is Marbella_f03.dll+{:x}", Marbella_F03HUDInstructionScanResult - (std::uint8_t*)dllModule21);

					Memory::Write(Marbella_F03HUDInstructionScanResult + 6, iNewResX2);
				}
				else
				{
					spdlog::info("Cannot locate the HUD instruction memory address (Marbella_f03.dll).");
					return;
				}
			}
		}
		else if (name == L"marbella_f04.dll")
		{
			dllModule22 = moduleHandle;

			if (loaded)
			{
				std::uint8_t* Marbella_F04HUDInstructionScanResult = Memory::PatternScan(dllModule22, "68 58 02 00 00 68 20 03 00 00 8D 94 24 98 00 00 00 52 8D 84 24 98");
				if (Marbella_F04HUDInstructionScanResult)
				{
					spdlog::info("HUD Instruction: Address is Marbella_f04.dll+{:x}", Marbella_F04HUDInstructionScanResult - (std::uint8_t*)dllModule22);

					Memory::Write(Marbella_F04HUDInstructionScanResult + 6, iNewResX2);
				}
				else
				{
					spdlog::info("Cannot locate the HUD instruction memory address (Marbella_f04.dll).");
					return;
				}
			}
		}
		else if (name == L"marbella_malibu.dll")
		{
			dllModule23 = moduleHandle;

			if (loaded)
			{
				std::uint8_t* Marbella_MalibuHUDInstructionScanResult = Memory::PatternScan(dllModule23, "68 58 02 00 00 68 20 03 00 00 8D 8C 24 88 00 00 00 51 8D 94 24 88 00 00 00 52 6A 20");
				if (Marbella_MalibuHUDInstructionScanResult)
				{
					spdlog::info("HUD Instruction: Address is marbella_malibu.dll+{:x}", Marbella_MalibuHUDInstructionScanResult - (std::uint8_t*)dllModule23);

					Memory::Write(Marbella_MalibuHUDInstructionScanResult + 6, iNewResX2);
				}
				else
				{
					spdlog::info("Cannot locate the HUD instruction memory address (marbella_malibu.dll).");
					return;
				}
			}
		}
		else if (name == L"radar.dll")
		{
			dllModule24 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> RadarHUDInstructionsScansResult = Memory::PatternScan(dllModule24, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D6 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 6A ?? FF D6", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D6 B8", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 6A ?? FF 15", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 84 24");
				if (Memory::AreAllSignaturesValid(RadarHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is Radar.dll+{:x}", RadarHUDInstructionsScansResult[RadarHUD1Scan] - (std::uint8_t*)dllModule24);

					spdlog::info("HUD Instruction 2: Address is Radar.dll+{:x}", RadarHUDInstructionsScansResult[RadarHUD2Scan] - (std::uint8_t*)dllModule24);

					spdlog::info("HUD Instruction 3: Address is Radar.dll+{:x}", RadarHUDInstructionsScansResult[RadarHUD3Scan] - (std::uint8_t*)dllModule24);

					spdlog::info("HUD Instruction 4: Address is Radar.dll+{:x}", RadarHUDInstructionsScansResult[RadarHUD4Scan] - (std::uint8_t*)dllModule24);

					spdlog::info("HUD Instruction 5: Address is Radar.dll+{:x}", RadarHUDInstructionsScansResult[RadarHUD5Scan] - (std::uint8_t*)dllModule24);

					spdlog::info("HUD Instruction 6: Address is Radar.dll+{:x}", RadarHUDInstructionsScansResult[RadarHUD6Scan] - (std::uint8_t*)dllModule24);

					// HUD Instructions 1
					Memory::Write(RadarHUDInstructionsScansResult[RadarHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(RadarHUDInstructionsScansResult[RadarHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					Memory::Write(RadarHUDInstructionsScansResult[RadarHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					Memory::Write(RadarHUDInstructionsScansResult[RadarHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					Memory::Write(RadarHUDInstructionsScansResult[RadarHUD5Scan] + 6, iNewResX2);

					// HUD Instructions 6
					Memory::Write(RadarHUDInstructionsScansResult[RadarHUD6Scan] + 6, iNewResX2);
				}
			}
		}
		else if (name == L"script.dll")
		{
			dllModule25 = moduleHandle;

			if (loaded)
			{
				std::vector<std::uint8_t*> ScriptHUDInstructionsScansResult = Memory::PatternScan(dllModule25, "68 58 02 00 00 68 20 03 00 00 8B F9 8D 44 24 24 50 8D 4C 24", "68 58 02 00 00 68 20 03 00 00 8B D9 8D 44 24 20 50 8D 4C 24 28 51 68 00 01", "68 58 02 00 00 68 20 03 00 00 8D 44 24 18 50 8D 4C 24 20 51 6A 0A 6A 64 FF D3 8B 54", "68 58 02 00 00 68 20 03 00 00 8D 54 24 20 52 8D 44 24 28 50 68 00 01 00 00 68 00 01 00 00 FF D3", "68 58 02 00 00 68 20 03 00 00 8D 4C 24 2C 51 8D 54 24 2C 52 6A 27 6A 23 FF D3 8B 5C 24 2C 8B");
				if (Memory::AreAllSignaturesValid(ScriptHUDInstructionsScansResult) == true)
				{
					spdlog::info("HUD Instruction 1: Address is Script.dll+{:x}", ScriptHUDInstructionsScansResult[ScriptHUD1Scan] - (std::uint8_t*)dllModule25);

					spdlog::info("HUD Instruction 2: Address is Script.dll+{:x}", ScriptHUDInstructionsScansResult[ScriptHUD2Scan] - (std::uint8_t*)dllModule25);

					spdlog::info("HUD Instruction 3: Address is Script.dll+{:x}", ScriptHUDInstructionsScansResult[ScriptHUD3Scan] - (std::uint8_t*)dllModule25);

					spdlog::info("HUD Instruction 4: Address is Script.dll+{:x}", ScriptHUDInstructionsScansResult[ScriptHUD4Scan] - (std::uint8_t*)dllModule25);

					spdlog::info("HUD Instruction 5: Address is Script.dll+{:x}", ScriptHUDInstructionsScansResult[ScriptHUD5Scan] - (std::uint8_t*)dllModule25);

					// HUD Instructions 1
					Memory::Write(ScriptHUDInstructionsScansResult[ScriptHUD1Scan] + 6, iNewResX2);

					// HUD Instructions 2
					Memory::Write(ScriptHUDInstructionsScansResult[ScriptHUD2Scan] + 6, iNewResX2);

					// HUD Instructions 3
					Memory::Write(ScriptHUDInstructionsScansResult[ScriptHUD3Scan] + 6, iNewResX2);

					// HUD Instructions 4
					Memory::Write(ScriptHUDInstructionsScansResult[ScriptHUD4Scan] + 6, iNewResX2);

					// HUD Instructions 5
					Memory::Write(ScriptHUDInstructionsScansResult[ScriptHUD5Scan] + 6, iNewResX2);
				}
			}
		}
	}	
}

void SetValues()
{
	fNewAspectRatio = static_cast<float>(iNewResX) / static_cast<float>(iNewResY);

	fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

	iNewResX2 = (int)(800.0f * fAspectRatioScale);
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
		SetValues();
	}

	g_hWatcherThread = CreateThread(NULL, 0, DllWatcherThread, NULL, 0, NULL);
	if (g_hWatcherThread)
	{
		SetThreadPriority(g_hWatcherThread, THREAD_PRIORITY_NORMAL);
		CloseHandle(g_hWatcherThread);
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