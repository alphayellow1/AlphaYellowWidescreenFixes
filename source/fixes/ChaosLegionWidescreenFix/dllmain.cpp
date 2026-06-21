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
#include <cstdio>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

HMODULE exeModule = GetModuleHandle(NULL);
HMODULE thisModule;

// Fix details
std::string sFixName = "ChaosLegionWidescreenFix";
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

// Variables
int iCurrentResX;
int iCurrentResY;
float fNewAspectRatio;
float fNewRenderingSidesValue;

// Game detection
enum class Game
{
	CL,
	Unknown
};

struct GameInfo
{
	std::string GameTitle;
	std::string ExeName;
};

const std::map<Game, GameInfo> kGames = {
	{Game::CL, {"Chaos Legion", "ChaosLegion.exe"}},
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

#include <string>
#include <algorithm>

struct ResolutionMode {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
};

static uintptr_t g_base = 0;

static SafetyHookMid g_resolution_table_mid{};
static SafetyHookMid g_width_cap_mid{};
static SafetyHookMid g_height_cap_mid{};
static SafetyHookMid g_capability_mid{};
static SafetyHookMid g_input_begin_mid{};
static SafetyHookMid g_input_end_mid{};

static std::vector<ResolutionMode> g_system_modes{};

static int g_page_start = 0;
static int g_old_menu_index = 0;
static bool g_rebuilding = false;

static constexpr int MENU_SLOT_COUNT = 20;

static uintptr_t rva(uintptr_t off) {
    return g_base + off;
}

static void write_mem(uintptr_t addr, const void* data, size_t size) {
    DWORD old{};
    VirtualProtect(reinterpret_cast<void*>(addr), size, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(reinterpret_cast<void*>(addr), data, size);
    VirtualProtect(reinterpret_cast<void*>(addr), size, old, &old);
}

static void write_u32(uintptr_t addr, uint32_t value) {
    write_mem(addr, &value, sizeof(value));
}

static void write_u8(uintptr_t addr, uint8_t value) {
    write_mem(addr, &value, sizeof(value));
}

static void write_string(uintptr_t addr, const char* text, size_t max_len)
{
    if (!text || max_len == 0)
    {
        return;
    }

    DWORD old{};
    VirtualProtect(reinterpret_cast<void*>(addr), max_len, PAGE_EXECUTE_READWRITE, &old);

    char* dst = reinterpret_cast<char*>(addr);

    std::memset(dst, 0, max_len);
    strncpy_s(dst, max_len, text, _TRUNCATE);

    VirtualProtect(reinterpret_cast<void*>(addr), max_len, old, &old);
}

static uint8_t& selected_resolution_index() {
    return *reinterpret_cast<uint8_t*>(rva(0x232690));
}

static void enumerate_system_resolutions() {
    g_system_modes.clear();

    DEVMODEA dm{};
    dm.dmSize = sizeof(dm);

    for (DWORD i = 0; EnumDisplaySettingsA(nullptr, i, &dm); i++) {
        if (dm.dmPelsWidth == 0 || dm.dmPelsHeight == 0) {
            continue;
        }

        if (dm.dmBitsPerPel < 16) {
            continue;
        }

        ResolutionMode mode{
            static_cast<uint32_t>(dm.dmPelsWidth),
            static_cast<uint32_t>(dm.dmPelsHeight),
            static_cast<uint32_t>(dm.dmBitsPerPel)
        };

        auto exists = std::find_if(
            g_system_modes.begin(),
            g_system_modes.end(),
            [&](const ResolutionMode& m) {
                return m.width == mode.width &&
                    m.height == mode.height &&
                    m.bpp == mode.bpp;
            }
        );

        if (exists == g_system_modes.end()) {
            g_system_modes.push_back(mode);
        }
    }

    std::sort(
        g_system_modes.begin(),
        g_system_modes.end(),
        [](const ResolutionMode& a, const ResolutionMode& b) {
            if (a.width != b.width) {
                return a.width < b.width;
            }

            if (a.height != b.height) {
                return a.height < b.height;
            }

            return a.bpp < b.bpp;
        }
    );
}

static void write_resolution_slot(int slot, const ResolutionMode* mode) {
    /*
        Game hardcoded resolution table:

            entry_base = ChaosLegion.exe + 0x1A61D8 + slot * 0x130

        Known fields:

            +0x00 = width
            +0x04 = height
            +0x2C = mode flag / color flag used by +4CA0 and +58F0
            +0x2D = visible text string

        Runtime availability table:

            ChaosLegion.exe + 0x232500 + slot * 0x0C

        Important:
        Do not only force 232500 availability bytes.
        Let the game fill 232500 via function +5640.
    */

    constexpr uintptr_t RES_TABLE_BASE = 0x1A61D8;
    constexpr uintptr_t RES_ENTRY_SIZE = 0x130;
    constexpr uintptr_t RES_LABEL_OFF = 0x2D;

    const uintptr_t entry = rva(RES_TABLE_BASE + slot * RES_ENTRY_SIZE);

    if (!mode) {
        write_u32(entry + 0x00, 0);
        write_u32(entry + 0x04, 0);
        write_string(entry + RES_LABEL_OFF, "", 0x100);
        return;
    }

    write_u32(entry + 0x00, mode->width);
    write_u32(entry + 0x04, mode->height);

    // Keep this simple and compatible with the game's old labels.
    char label[64]{};
    std::snprintf(
        label,
        sizeof(label),
        "%uX%uX%u(FULL)",
        mode->width,
        mode->height,
        mode->bpp
    );

    write_string(entry + RES_LABEL_OFF, label, 0x100);
}

static void write_current_resolution_page() {
    if (g_system_modes.empty()) {
        return;
    }

    if (g_page_start < 0) {
        g_page_start = 0;
    }

    if (g_page_start >= static_cast<int>(g_system_modes.size())) {
        g_page_start = std::max(0, static_cast<int>(g_system_modes.size()) - 1);
    }

    for (int slot = 0; slot < MENU_SLOT_COUNT; slot++) {
        const int mode_index = g_page_start + slot;

        if (mode_index >= 0 && mode_index < static_cast<int>(g_system_modes.size())) {
            write_resolution_slot(slot, &g_system_modes[mode_index]);
        }
        else {
            write_resolution_slot(slot, nullptr);
        }
    }
}

static void rebuild_game_resolution_table() {
    if (g_rebuilding) {
        return;
    }

    g_rebuilding = true;

    using RebuildFn = void(__cdecl*)();
    auto fn = reinterpret_cast<RebuildFn>(rva(0x5640));

    fn();

    g_rebuilding = false;
}

void install_resolution_hooks() {
    g_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"ChaosLegion.exe"));

    if (!g_base) {
        return;
    }

    enumerate_system_resolutions();

    /*
        Hook +5640.

        This is the game function that populates:

            ChaosLegion.exe+232500

        We overwrite the 20 hardcoded menu entries with a page of real system modes
        right before the game validates them.
    */
    g_resolution_table_mid = safetyhook::create_mid(
        reinterpret_cast<uint8_t*>(rva(0x5640)),
        [](SafetyHookContext& ctx) {
            write_current_resolution_page();
        }
    );

    /*
      cap:

            +566F jg +570C

        Original rejects if table_width > [230BBC].
        With dynamic system modes, do not let desktop/current cap hide valid modes.
    */
    g_width_cap_mid = safetyhook::create_mid(
        reinterpret_cast<uint8_t*>(rva(0x566F)),
        [](SafetyHookContext& ctx) {
            ctx.eip = static_cast<uint32_t>(rva(0x5675));
        }
    );

    /*
        Skip height cap:

            +567F jg +570C

        Original rejects if table_height > [230BB8].
    */
    g_height_cap_mid = safetyhook::create_mid(
        reinterpret_cast<uint8_t*>(rva(0x567F)),
        [](SafetyHookContext & ctx){
            ctx.eip = static_cast<uint32_t>(rva(0x5685));
        }
    );

    /*
        Optional: skip capability-level rejection:

            +5699 jl +570C

        This prevents the old hardcoded table's capability values from rejecting
        your injected system modes.
    */
    g_capability_mid = safetyhook::create_mid(
        reinterpret_cast<uint8_t*>(rva(0x5699)),
        [](SafetyHookContext& ctx) {
            ctx.eip = static_cast<uint32_t>(rva(0x569B));
        }
    );

    /*
        Hook start of resolution input handler +5750.

        Save the previous visible slot index before the game processes left/right input.
    */
    g_input_begin_mid = safetyhook::create_mid(
        reinterpret_cast<uint8_t*>(rva(0x5750)),
        [](SafetyHookContext& ctx) {
            g_old_menu_index = selected_resolution_index();
        }
    );

    /*
        Hook near the end of input handler +584A.

        After the game changes selected slot, detect edge movement.

        If user moves past the last visible slot, advance the page.
        If user moves before the first visible slot, go back a page.

        The game still only sees 20 slots, but our backing vector can contain
        every system resolution.
    */
    g_input_end_mid = safetyhook::create_mid(
        reinterpret_cast<uint8_t*>(rva(0x584A)),
        [](SafetyHookContext& ctx) {
            if (g_system_modes.empty()) {
                return;
            }

            int current = selected_resolution_index();

            const int max_page_start =
                std::max(0, static_cast<int>(g_system_modes.size()) - MENU_SLOT_COUNT);

            bool changed_page = false;

            // Moving right at the end of the 20-slot window.
            if (g_old_menu_index == MENU_SLOT_COUNT - 2 &&
                current == MENU_SLOT_COUNT - 1 &&
                g_page_start < max_page_start) {
                g_page_start++;
                selected_resolution_index() = MENU_SLOT_COUNT - 2;
                changed_page = true;
            }

            // Moving left at the start of the 20-slot window.
            if (g_old_menu_index == 1 &&
                current == 0 &&
                g_page_start > 0) {
                g_page_start--;
                selected_resolution_index() = 1;
                changed_page = true;
            }

            if (changed_page) {
                write_current_resolution_page();
                rebuild_game_resolution_table();
            }
        }
    );
}

void WidescreenFix()
{
    if (eGameType != Game::CL || bFixActive != true)
    {
        return;
    }

    spdlog::info("Installing Chaos Legion widescreen/resolution fix...");

    if (iCurrentResX > 0 && iCurrentResY > 0)
    {
        fNewAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);
        fNewRenderingSidesValue = fNewAspectRatio / fOldAspectRatio;

        spdlog::info("Target resolution: {}x{}", iCurrentResX, iCurrentResY);
        spdlog::info("New aspect ratio: {}", fNewAspectRatio);
        spdlog::info("Rendering sides multiplier: {}", fNewRenderingSidesValue);
    }

    install_resolution_hooks();

    spdlog::info("Resolution hooks installed.");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		thisModule = hModule;
		Logging();
		Configuration();
		if (DetectGame())
		{
            WidescreenFix();
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