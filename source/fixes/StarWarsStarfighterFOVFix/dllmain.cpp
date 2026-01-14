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
#include <cmath>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;
HMODULE dllModule2 = nullptr;
HMODULE dllModule3 = nullptr;
HMODULE dllModule4 = nullptr;
std::string sDllName1;
std::string sDllName2;
std::string sDllName3;

// Fix details
std::string sFixName = "StarWarsStarfighterFOVFix";
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
float fNewLocalToScreenFOV;
float fUnzoomedFOVFactor;
float fZoomedFOVFactor;

// Variables
float fNewAspectRatio;
float fAspectRatioScale;
float fNewCameraFOV;
int iNewResX;
float fNewFrustumFOV;
float fNewRendererViewFOV;
float fNewUnzoomedFOV;
float fNewZoomedFOV;

// Game detection
enum class Game
{
	SWS,
	Unknown
};

enum CameraFOVInstructionsIndices
{
	FrustumFOVScan,
	LocalToScreenFOVScan,
	UnzoomedFOVScan,
	ZoomedFOVScan,
	RendererViewFOVScan
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::SWS, {"Star Wars: Starfighter", "Europa.exe"}},
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
	inipp::get_value(ini.sections["Settings"], "UnzoomedFOVFactor", fUnzoomedFOVFactor);
	inipp::get_value(ini.sections["Settings"], "ZoomedFOVFactor", fZoomedFOVFactor);
	spdlog_confparse(iCurrentResX);
	spdlog_confparse(iCurrentResY);
	spdlog_confparse(fUnzoomedFOVFactor);
	spdlog_confparse(fZoomedFOVFactor);

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

	dllModule2 = Memory::GetHandle("LEC3DEngine.dll");

	dllModule3 = Memory::GetHandle("LECGame.dll");

	dllModule4 = Memory::GetHandle("LECDirect3D.dll");

	sDllName1 = Memory::GetModuleName(dllModule2);

	sDllName2 = Memory::GetModuleName(dllModule3);

	sDllName3 = Memory::GetModuleName(dllModule4);

	return true;
}

static SafetyHookMid AspectRatioInstructionHook{};
static SafetyHookMid FrustumFOVHook{};
static SafetyHookMid LocalToScreenFOVHook{};
static SafetyHookMid UnzoomedFOVHook{};
static SafetyHookMid ZoomedFOVHook{};
static SafetyHookMid RendererViewFOVHook{};

void FOVFix()
{
	if (eGameType == Game::SWS && bFixActive == true)
	{
		fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

		fAspectRatioScale = fNewAspectRatio / fOldAspectRatio;

		// Located in LEC3DEngine.CPanel::GetView
		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(dllModule2, "DA 74 24");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {}+{:x}", sDllName1, AspectRatioInstructionScanResult - (std::uint8_t*)dllModule2);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 4);

			AspectRatioInstructionHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentResX2 = *reinterpret_cast<int*>(ctx.esp + 0x4);

				iNewResX = (int)(iCurrentResX2 * fAspectRatioScale);

				FPU::FIDIV(iNewResX);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		// Instructions are located in LEC3DEngine.CFrustum::SetupPerspective, LEC3DEngine.CView::LocalToScreen, LECGame.CViewManager::SetUnzoomedFOV, LECGame.CViewManager::SetZoomedFOV and LECDirect3D.CD3DRenderer::SetView
		std::vector<std::uint8_t*> CameraFOVInstructionsScansResult = Memory::PatternScan(
		dllModule2, "D9 46 ?? D8 0D", "D9 41 ?? D8 0D",
		dllModule3, "8B 44 24 ?? A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? C2 ?? ?? D9 05 ?? ?? ?? ?? C3 8B 44 24", "8B 44 24 ?? A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? C2 ?? ?? D9 05 ?? ?? ?? ?? C3 53",
		dllModule4, "D9 46 ?? D8 0D"
		);
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Frustum FOV Instruction: Address is {}+{:x}", sDllName1, CameraFOVInstructionsScansResult[FrustumFOVScan] - (std::uint8_t*)dllModule2);

			spdlog::info("Local to Screen FOV Instruction: Address is {}+{:x}", sDllName1, CameraFOVInstructionsScansResult[LocalToScreenFOVScan] - (std::uint8_t*)dllModule2);

			spdlog::info("Unzoomed FOV Instruction: Address is {}+{:x}", sDllName2, CameraFOVInstructionsScansResult[UnzoomedFOVScan] - (std::uint8_t*)dllModule3);

			spdlog::info("Zoomed FOV Instruction: Address is {}+{:x}", sDllName2, CameraFOVInstructionsScansResult[ZoomedFOVScan] - (std::uint8_t*)dllModule3);

			spdlog::info("Renderer View FOV Instruction: Address is {}+{:x}", sDllName3, CameraFOVInstructionsScansResult[RendererViewFOVScan] - (std::uint8_t*)dllModule4);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FrustumFOVScan], 3);

			FrustumFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FrustumFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentFrustumFOV = *reinterpret_cast<float*>(ctx.esi + 0x78);

				fNewFrustumFOV = Maths::CalculateNewFOV_RadBased(fCurrentFrustumFOV, fAspectRatioScale);

				FPU::FLD(fNewFrustumFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[LocalToScreenFOVScan], 3);

			LocalToScreenFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[LocalToScreenFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentLocalToScreenFOV = *reinterpret_cast<float*>(ctx.ecx + 0x78);

				fNewLocalToScreenFOV = Maths::CalculateNewFOV_RadBased(fCurrentLocalToScreenFOV, fAspectRatioScale);

				FPU::FLD(fNewLocalToScreenFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[UnzoomedFOVScan], 4);

			UnzoomedFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[UnzoomedFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentUnzoomedFOV = *reinterpret_cast<float*>(ctx.esp + 0x4);

				fNewUnzoomedFOV = fCurrentUnzoomedFOV * fUnzoomedFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewUnzoomedFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[ZoomedFOVScan], 4);

			ZoomedFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[ZoomedFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomedFOV = *reinterpret_cast<float*>(ctx.esp + 0x4);

				fNewZoomedFOV = fCurrentZoomedFOV / fZoomedFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(fNewZoomedFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[RendererViewFOVScan], 3);

			RendererViewFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[RendererViewFOVScan], [](SafetyHookContext& ctx)
			{
				float& fCurrentRendererViewFOV = *reinterpret_cast<float*>(ctx.esi + 0x78);

				fNewRendererViewFOV = Maths::CalculateNewFOV_RadBased(fCurrentRendererViewFOV, fAspectRatioScale);

				FPU::FLD(fNewRendererViewFOV);
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