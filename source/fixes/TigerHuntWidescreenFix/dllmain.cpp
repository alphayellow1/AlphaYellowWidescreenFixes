#include "..\..\common\FixBase.hpp"

class TigerHuntFix final : public FixBase
{
public:
    explicit TigerHuntFix(HMODULE selfModule) : FixBase(selfModule)
    {
        s_instance_ = this;
    }

    ~TigerHuntFix() override
    {
        if (s_instance_ == this)
        {
            s_instance_ = nullptr;
        }
    }

    void RestoreResolutionInConfIni()
    {
        if (m_confIniPathAbsolute.empty())
        {
            return;
        }

        if (m_hasIniXRes)
        {
            WritePrivateProfileStringA(
                "OT",
                "XRes",
                std::to_string(m_iniXRes).c_str(),
                m_confIniPathAbsolute.c_str()
            );
        }

        if (m_hasIniYRes)
        {
            WritePrivateProfileStringA(
                "OT",
                "YRes",
                std::to_string(m_iniYRes).c_str(),
                m_confIniPathAbsolute.c_str()
            );
        }

        spdlog::info("Restored conf.ini resolution to {}x{}", m_iniXRes, m_iniYRes);
    }

protected:
    const char* FixName() const override
    {
        return "TigerHuntWidescreenFix";
    }

    const char* FixVersion() const override
    {
        return "1.0";
    }

    const char* TargetName() const override
    {
        return "Tiger Hunt";
    }

    InitMode GetInitMode() const override
    {
        // return InitMode::Direct;
        return InitMode::WorkerThread;
        // return InitMode::ExportedOnly;
    }

    bool IsCompatibleExecutable(const std::string& exeName) const override
    {
        return Util::stringcmp_caseless(exeName, "th.ews") ||
        Util::stringcmp_caseless(exeName, "thmap.ews");
    }

    void ParseFixConfig(inipp::Ini<char>& ini) override
    {
        /*
        inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
        spdlog_confparse(m_fovFactor);
        */
    }

    void ApplyFix() override
    {
        LoadGameConfig();

        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

        if (kernel32 != nullptr)
        {
            auto* writePrivateProfileStringA = reinterpret_cast<std::uint8_t*>(GetProcAddress(kernel32, "WritePrivateProfileStringA"));

            if (writePrivateProfileStringA != nullptr)
            {
                m_writePrivateProfileStringHook = safetyhook::create_inline(writePrivateProfileStringA, WritePrivateProfileStringAHook);

                if (m_writePrivateProfileStringHook)
                {
                    s_writePrivateProfileStringAOriginal = m_writePrivateProfileStringHook.original<WritePrivateProfileStringA_t>();
                }
                else
                {
                    spdlog::error("Failed to hook WritePrivateProfileStringA.");
                }
            }
        }

        const bool isMainExe = Util::stringcmp_caseless(ExeName(), "th.ews");
        const bool isMapExe = Util::stringcmp_caseless(ExeName(), "thmap.ews");

        if (isMainExe == true)
        {
            std::uint8_t* const scan = Memory::PatternScan(ExeModule(), "99 53 68 ?? ?? ?? ?? 83 E2 03 03 C2 6A 10 C1 F8 02 68 ?? ?? ?? ?? 8D 04 40 68 ?? ?? ?? ?? A3 ?? ?? ?? ?? FF D6");

            if (scan != nullptr)
            {
                std::uint8_t* const calcStart = scan;
                std::uint8_t* const yResStore = calcStart + 0x1E;

                if (*(calcStart - 0x05) == 0xA3)
                {
                    m_mainXResGlobal = *reinterpret_cast<std::uint32_t**>(calcStart - 0x04);
                }
                else
                {
                    spdlog::warn("Main XRes global could not be resolved.");
                }

                m_mainConfIniPath = *reinterpret_cast<const char**>(calcStart + 0x03);
                m_mainIniSection = *reinterpret_cast<const char**>(calcStart + 0x1A);
                m_mainYResGlobal = *reinterpret_cast<std::uint32_t**>(yResStore + 0x01);

                m_resolutionHook = safetyhook::create_mid(yResStore, MainResolutionHook);
            }
            else
            {
                spdlog::error("Failed to find main resolution YRes store pattern.");
            }
        }

        if (isMapExe == true)
        {
            std::uint8_t* const scan = Memory::PatternScan(ExeModule(), "8D 04 40 99 68 ?? ?? ?? ?? 83 E2 03 6A 01 03 C2 68 ?? ?? ?? ?? C1 F8 02 68 ?? ?? ?? ?? A3 ?? ?? ?? ?? FF D6");

            if (scan != nullptr)
            {
                std::uint8_t* const calcStart = scan;
                std::uint8_t* const yResStore = calcStart + 0x1D;

                if (*(calcStart - 0x05) == 0xA3)
                {
                    m_mapXResGlobal = *reinterpret_cast<std::uint32_t**>(calcStart - 0x04);
                }
                else
                {
                    spdlog::warn("Map XRes global could not be resolved.");
                }

                m_mapConfIniPath = *reinterpret_cast<const char**>(calcStart + 0x05);
                m_mapIniSection = *reinterpret_cast<const char**>(calcStart + 0x19);
                m_mapYResGlobal = *reinterpret_cast<std::uint32_t**>(yResStore + 0x01);

                m_mapResolutionHook = safetyhook::create_mid(yResStore, MapResolutionHook);
            }
            else
            {
                spdlog::error("Failed to find map resolution YRes store pattern.");
            }
        }
    }

private:
    SafetyHookMid m_resolutionHook{};
    SafetyHookMid m_mapResolutionHook{};

    static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

    const char* m_mainConfIniPath = nullptr;
    const char* m_mainIniSection = nullptr;

    std::uint32_t* m_mainXResGlobal = nullptr;
    std::uint32_t* m_mainYResGlobal = nullptr;

    std::uint32_t* m_mapXResGlobal = nullptr;
    std::uint32_t* m_mapYResGlobal = nullptr;

    const char* m_mapConfIniPath = nullptr;
    const char* m_mapIniSection = nullptr;

    std::string m_confIniPathAbsolute{};

    int m_iniXRes = 0;
    int m_iniYRes = 0;

    bool m_hasIniXRes = false;
    bool m_hasIniYRes = false;

    void LoadGameConfig()
    {
        char exePath[MAX_PATH]{};
        GetModuleFileNameA(reinterpret_cast<HMODULE>(ExeModule()), exePath, MAX_PATH);

        m_confIniPathAbsolute = (std::filesystem::path(exePath).parent_path() / "conf.ini").string();

        std::ifstream file(m_confIniPathAbsolute);

        if (!file.is_open())
        {
            spdlog::error("Failed to open game conf.ini: {}", m_confIniPathAbsolute);
            return;
        }

        inipp::Ini<char> gameIni;
        gameIni.parse(file);

        auto sectionIt = gameIni.sections.find("OT");

        if (sectionIt == gameIni.sections.end())
        {
            spdlog::error("Could not find [OT] section in {}", m_confIniPathAbsolute);
            return;
        }

        auto& ot = sectionIt->second;

        if (ot.find("XRes") != ot.end())
        {
            inipp::get_value(ot, "XRes", m_iniXRes);
            m_hasIniXRes = m_iniXRes > 0;
        }

        if (ot.find("YRes") != ot.end())
        {
            inipp::get_value(ot, "YRes", m_iniYRes);
            m_hasIniYRes = m_iniYRes > 0;
        }
    }

    inline static TigerHuntFix* s_instance_ = nullptr;

    static void MainResolutionHook(SafetyHookContext& ctx)
    {
        TigerHuntFix* const self = s_instance_;

        if (self == nullptr)
        {
            return;
        }

        int finalXRes = 0;
        int finalYRes = 0;

        if (self->m_hasIniXRes)
        {
            finalXRes = self->m_iniXRes;
        }
        else if (self->m_mainXResGlobal != nullptr && *self->m_mainXResGlobal >= 320)
        {
            finalXRes = static_cast<int>(*self->m_mainXResGlobal);
        }
        else
        {
            finalXRes = 640;
        }

        if (self->m_hasIniYRes)
        {
            finalYRes = self->m_iniYRes;
        }
        else
        {
            finalYRes = (finalXRes / 4) * 3;
        }

        if (finalXRes < 320)
        {
            finalXRes = 640;
        }

        if (finalYRes < 200)
        {
            finalYRes = 480;
        }

        if (self->m_mainXResGlobal != nullptr)
        {
            *self->m_mainXResGlobal = static_cast<std::uint32_t>(finalXRes);
        }

        ctx.eax = static_cast<std::uintptr_t>(finalYRes);
    }

    static void MapResolutionHook(SafetyHookContext& ctx)
    {
        TigerHuntFix* const self = s_instance_;

        if (self == nullptr)
        {
            return;
        }

        int finalXRes = 0;
        int finalYRes = 0;

        if (self->m_hasIniXRes)
        {
            finalXRes = self->m_iniXRes;
        }
        else if (self->m_mapXResGlobal != nullptr && *self->m_mapXResGlobal >= 320)
        {
            finalXRes = static_cast<int>(*self->m_mapXResGlobal);
        }
        else
        {
            finalXRes = 640;
        }

        if (self->m_hasIniYRes)
        {
            finalYRes = self->m_iniYRes;
        }
        else
        {
            finalYRes = (finalXRes * 3) / 4;
        }

        if (finalXRes < 320)
        {
            finalXRes = 640;
        }

        if (finalYRes < 200)
        {
            finalYRes = 480;
        }

        if (self->m_mapXResGlobal != nullptr)
        {
            *self->m_mapXResGlobal = static_cast<std::uint32_t>(finalXRes);
        }

        ctx.eax = static_cast<std::uintptr_t>(finalYRes);
    }

    SafetyHookInline m_writePrivateProfileStringHook{};

    using WritePrivateProfileStringA_t = BOOL(WINAPI*)(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString, LPCSTR lpFileName);

    inline static WritePrivateProfileStringA_t s_writePrivateProfileStringAOriginal = nullptr;

    static bool StringEqualsIgnoreCase(const char* a, const char* b)
    {
        if (a == nullptr || b == nullptr)
        {
            return false;
        }

        return _stricmp(a, b) == 0;
    }

    static bool PathLooksLikeConfIni(const char* path)
    {
        if (path == nullptr)
        {
            return false;
        }

        std::filesystem::path p(path);
        return StringEqualsIgnoreCase(p.filename().string().c_str(), "conf.ini");
    }

    static int StringToIntSafe(const char* text, int fallback)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return fallback;
        }

        char* end = nullptr;
        const long value = std::strtol(text, &end, 10);

        if (end == text)
        {
            return fallback;
        }

        return static_cast<int>(value);
    }

    static BOOL WINAPI WritePrivateProfileStringAHook(LPCSTR lpAppName, LPCSTR lpKeyName, LPCSTR lpString, LPCSTR lpFileName)
    {
        TigerHuntFix* const self = s_instance_;

        if (self == nullptr || s_writePrivateProfileStringAOriginal == nullptr)
        {
            return FALSE;
        }

        const bool isConfIni = PathLooksLikeConfIni(lpFileName);
        const bool isOTSection = StringEqualsIgnoreCase(lpAppName, "OT");

        if (isConfIni && isOTSection && lpKeyName != nullptr)
        {
            if (StringEqualsIgnoreCase(lpKeyName, "XRes"))
            {
                const int attemptedXRes = StringToIntSafe(lpString, 0);

                if (attemptedXRes <= 0 && self->m_hasIniXRes)
                {
                    char fixedValue[32]{};
                    std::snprintf(fixedValue, sizeof(fixedValue), "%d", self->m_iniXRes);

                    return s_writePrivateProfileStringAOriginal(lpAppName, lpKeyName, fixedValue, lpFileName);
                }
            }

            if (StringEqualsIgnoreCase(lpKeyName, "YRes"))
            {
                const int attemptedYRes = StringToIntSafe(lpString, 0);

                if (attemptedYRes <= 0 && self->m_hasIniYRes)
                {
                    char fixedValue[32]{};
                    std::snprintf(fixedValue, sizeof(fixedValue), "%d", self->m_iniYRes);

                    return s_writePrivateProfileStringAOriginal(lpAppName, lpKeyName, fixedValue, lpFileName);
                }
            }
        }

        return s_writePrivateProfileStringAOriginal(lpAppName, lpKeyName, lpString, lpFileName);
    }
};

static std::unique_ptr<TigerHuntFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<TigerHuntFix>(hModule);
		g_fix->Start();
		break;
	}

	case DLL_PROCESS_DETACH:
	{
		g_fix->Shutdown();
        g_fix->RestoreResolutionInConfIni();
		g_fix.reset();
		break;
	}

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}

	return TRUE;
}