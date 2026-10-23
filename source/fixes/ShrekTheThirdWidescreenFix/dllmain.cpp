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
std::string sFixName = "ShrekTheThirdWidescreenFix";
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
constexpr float fOriginalCameraFOV = 60.0f;

// Ini variables
bool bFixActive;
int iCurrentResX;
int iCurrentResY;
float fFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;

// Game detection
enum class Game
{
	STT,
	Unknown
};

enum ResolutionInstructionsIndices
{
	Res1,
	Res2
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::STT, {"Shrek the Third", "SHReK the THiRD.exe"}},
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

static SafetyHookMid ResolutionInstructions1Hook{};
static SafetyHookMid ResolutionInstructions2Hook{};
static SafetyHookMid CameraFOVInstructionHook{};

void WidescreenFix()
{
	if (eGameType == Game::STT && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		std::vector<std::uint8_t*> ResolutionListScanResult = Memory::PatternScan(exeModule, "89 2D ?? ?? ?? ?? 89 15", "89 35 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 35 ?? ?? ?? ?? BE");
		if (Memory::AreAllSignaturesValid(ResolutionListScanResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult[Res1] - (std::uint8_t*)exeModule);

			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionListScanResult[Res2] - (std::uint8_t*)exeModule);

			ResolutionInstructions1Hook = safetyhook::create_mid(ResolutionListScanResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.edx = std::bit_cast<uintptr_t>(iCurrentResY);
			});

			ResolutionInstructions2Hook = safetyhook::create_mid(ResolutionListScanResult[Res2], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(iCurrentResX);

				ctx.eax = std::bit_cast<uintptr_t>(iCurrentResY);
			});
		}

		// The game does not create its device from the resolution globals above. It picks a
		// mode out of a hardcoded 5-entry table ({width, height, 1} each, followed by the
		// entry count) and the "resolution" value in its settings file is an index into it.
		// The hooks above rewrite the globals, but the device is still built from the indexed
		// table entry, so the two disagree and the frame comes out black - which is the
		// black screen reported in issue #77.
		//
		// Rewriting the table makes the wanted mode a real member of the game's own list, so
		// it is selected on every path, including the one taken when no settings file exists
		// yet. It also gets past the resolution validator at exe+0x18F3C4, which resets any
		// mode that is not in the list.
		//
		// All five entries are set: the index then cannot select a wrong one, so this works
		// with no settings file at all. (Rewriting a single entry only works for the last
		// one - the list appears to need ascending order.)
		std::uint8_t* ResolutionModeTableScanResult = Memory::PatternScan(exeModule,
			"80 02 00 00 E0 01 00 00 01 00 00 00 20 03 00 00 58 02 00 00 01 00 00 00 "
			"00 04 00 00 00 03 00 00 01 00 00 00 00 05 00 00 00 04 00 00 01 00 00 00 "
			"40 06 00 00 B0 04 00 00 01 00 00 00 05 00 00 00");
		if (ResolutionModeTableScanResult)
		{
			spdlog::info("Resolution Mode Table Scan: Address is {:s}+{:x}", sExeName.c_str(), ResolutionModeTableScanResult - (std::uint8_t*)exeModule);

			for (int i = 0; i < 5; ++i)
			{
				Memory::Write(ResolutionModeTableScanResult + (i * 12), iCurrentResX);
				Memory::Write(ResolutionModeTableScanResult + (i * 12) + 4, iCurrentResY);
			}
		}
		else
		{
			spdlog::error("Failed to locate resolution mode table memory address.");
			return;
		}

		// 2D UI scale. The function holding the Res1 instruction derives the UI scale at its
		// tail from its own stack arguments - the mode that was ASKED for - which the Res1
		// hook never touches:
		//
		//   fild [esp+0x18] ; fmul [1/200] ; fstp [uiScaleX]      = width/200
		//   fild [esp+0x0C] ; fmul [1/200] ; fstp [uiScaleY]      = height/200
		//   fld  [uiScaleX] ; fmul [0.3125]; fstp [uiScaleZ]      = width/640
		//
		// Sizing the UI off the WIDTH means that on a 16:9 display the menu text is scaled
		// for a 16:9 basis while the art is authored for 4:3, and it overflows. Worse, when
		// no settings file exists the game asks for its small default, gets the right mode,
		// and lays the menu out for the wrong one.
		//
		// The game already contains an unused override for this: at the scale function the
		// pair below is tested, and if non-zero the scale is taken from it instead. Both live
		// in .bss and nothing ever writes them, so the branch is always skipped. Setting them
		// to a 4:3-proportioned canvas at the real screen height gives the proportion the art
		// expects, and repointing the inline tail at the same globals keeps both writers in
		// agreement.
		std::uint8_t* UIScaleOverrideScanResult = Memory::PatternScan(exeModule,
			"8B 0D ?? ?? ?? ?? 85 C9 74 1A DB 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? DB 05 ?? ?? ?? ?? EB 12");
		std::uint8_t* UIScaleInlineScanResult = Memory::PatternScan(exeModule,
			"DB 44 24 18 5F 5E 5D D8 0D ?? ?? ?? ?? B0 01 5B D9 1D ?? ?? ?? ?? DB 44 24 0C "
			"D8 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? "
			"D9 1D ?? ?? ?? ?? 59 C2 08 00");
		if (UIScaleOverrideScanResult && UIScaleInlineScanResult)
		{
			spdlog::info("UI Scale Override Scan: Address is {:s}+{:x}", sExeName.c_str(), UIScaleOverrideScanResult - (std::uint8_t*)exeModule);
			spdlog::info("UI Scale Instructions Scan: Address is {:s}+{:x}", sExeName.c_str(), UIScaleInlineScanResult - (std::uint8_t*)exeModule);

			std::uint32_t uiOverrideWidthAddress = *reinterpret_cast<std::uint32_t*>(UIScaleOverrideScanResult + 2);
			std::uint32_t uiOverrideHeightAddress = *reinterpret_cast<std::uint32_t*>(UIScaleOverrideScanResult + 0x1E);

			std::uint32_t fOneOver200Address = *reinterpret_cast<std::uint32_t*>(UIScaleInlineScanResult + 0x09);
			std::uint32_t uiScaleXAddress = *reinterpret_cast<std::uint32_t*>(UIScaleInlineScanResult + 0x12);
			std::uint32_t uiScaleYAddress = *reinterpret_cast<std::uint32_t*>(UIScaleInlineScanResult + 0x22);
			std::uint32_t f0Point3125Address = *reinterpret_cast<std::uint32_t*>(UIScaleInlineScanResult + 0x2E);
			std::uint32_t uiScaleZAddress = *reinterpret_cast<std::uint32_t*>(UIScaleInlineScanResult + 0x34);

			// a 4:3-wide canvas at the real screen height
			Memory::Write(uiOverrideWidthAddress, static_cast<int>((iCurrentResY * 4) / 3));
			Memory::Write(uiOverrideHeightAddress, iCurrentResY);

			// Rewrite the inline tail to read the same override. It is 60 bytes and is
			// followed by 12 bytes of int3 alignment padding before the next function, so
			// the 64-byte replacement fits without relocating anything.
			std::uint8_t trampoline[64];
			std::size_t n = 0;
			auto emit = [&](std::uint8_t a, std::uint8_t b, std::uint32_t address)
			{
				trampoline[n++] = a;
				trampoline[n++] = b;
				*reinterpret_cast<std::uint32_t*>(trampoline + n) = address;
				n += 4;
			};
			emit(0xDB, 0x05, uiOverrideWidthAddress);   // fild dword ptr [overrideWidth]
			emit(0xD8, 0x0D, fOneOver200Address);       // fmul dword ptr [1/200]
			emit(0xD9, 0x1D, uiScaleXAddress);          // fstp dword ptr [uiScaleX]
			emit(0xDB, 0x05, uiOverrideHeightAddress);  // fild dword ptr [overrideHeight]
			emit(0xD8, 0x0D, fOneOver200Address);       // fmul dword ptr [1/200]
			emit(0xD9, 0x1D, uiScaleYAddress);          // fstp dword ptr [uiScaleY]
			emit(0xD9, 0x05, uiScaleXAddress);          // fld  dword ptr [uiScaleX]
			emit(0xD8, 0x0D, f0Point3125Address);       // fmul dword ptr [0.3125]
			emit(0xD9, 0x1D, uiScaleZAddress);          // fstp dword ptr [uiScaleZ]
			trampoline[n++] = 0x5F;                     // pop edi
			trampoline[n++] = 0x5E;                     // pop esi
			trampoline[n++] = 0x5D;                     // pop ebp
			trampoline[n++] = 0xB0; trampoline[n++] = 0x01;  // mov al, 1
			trampoline[n++] = 0x5B;                     // pop ebx
			trampoline[n++] = 0x59;                     // pop ecx
			trampoline[n++] = 0xC2; trampoline[n++] = 0x08; trampoline[n++] = 0x00;  // ret 8

			Memory::PatchBytes(UIScaleInlineScanResult, trampoline, n);
		}
		else
		{
			spdlog::error("Failed to locate UI scale memory address(es).");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(exeModule, "68 ?? ?? ?? ?? FF 52 ?? 8B 4C 24");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", sExeName.c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)exeModule);

			fNewCameraFOV = Maths::CalculateNewFOV_DegBased(fOriginalCameraFOV, fAspectRatioScale) * fFOVFactor;

			Memory::Write(CameraFOVInstructionScanResult + 1, fNewCameraFOV);
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
		break;
	}
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
