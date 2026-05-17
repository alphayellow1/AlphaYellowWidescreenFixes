#include "..\..\common\FixBase.hpp"
#include "..\..\common\DllNotificationWatcher.cpp"

#include <atomic>
#include <mutex>


class AliceFix final : public FixBase
{
public:
    explicit AliceFix(HMODULE selfModule) : FixBase(selfModule)
    {
        s_instance_ = this;
    }

    ~AliceFix() override
    {
        if (m_fgameWatcher)
        {
            m_fgameWatcher->Stop();
            m_fgameWatcher.reset();
        }

        if (s_instance_ == this)
        {
            s_instance_ = nullptr;
        }
    }

protected:
    const char* FixName() const override
    {
        return "AmericanMcGeesAliceFOVFix";
    }

    const char* FixVersion() const override
    {
        return "1.4";
    }

    const char* TargetName() const override
    {
        return "American McGee's Alice";
    }

    InitMode GetInitMode() const override
    {
        // return InitMode::Direct;
        return InitMode::WorkerThread;
        // return InitMode::ExportedOnly;
    }

    bool IsCompatibleExecutable(const std::string& exeName) const override
    {
        return Util::stringcmp_caseless(exeName, "alice.exe") || 
        Util::stringcmp_caseless(exeName, "quake3.exe");
    }

    void ParseFixConfig(inipp::Ini<char>& ini) override
    {
        inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
        spdlog_confparse(m_fovFactor);
    }

    void ApplyFix() override
    {
        auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 15 ?? ?? ?? ?? 83 C4 ?? 33 C0");
        if (ResolutionScanResult)
        {
            spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - reinterpret_cast<std::uint8_t*>(ExeModule()));

            m_resolutionWidthAddress = Memory::GetPointerFromAddress(ResolutionScanResult + 34, Memory::PointerMode::Absolute);
            m_resolutionHeightAddress = Memory::GetPointerFromAddress(ResolutionScanResult + 2, Memory::PointerMode::Absolute);

            m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
                {
                    int& iCurrentWidth = Memory::ReadMem(s_instance_->m_resolutionWidthAddress);
                    int& iCurrentHeight = Memory::ReadMem(s_instance_->m_resolutionHeightAddress);
                    s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
                    s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
                });
        }     
        else
        {
            spdlog::error("Failed to locate resolution instructions scan memory address.");
            return;
        }

        m_fgameWatcher = std::make_unique<DllNotificationWatcher>(
            // OnLoad
            [this](HMODULE module)
            {
                const std::string moduleName = Memory::GetModuleName(module);

                if (!Util::stringcmp_caseless(moduleName, "fgamex86.dll"))
                {
                    return;
                }

                m_dllModule2 = module;
                m_dllModule2Name = moduleName;

                spdlog::info("fgamex86.dll loaded.");

                WriteFOV();
            },
            // OnUnload
            [this](HMODULE module)
            {
                if (module != m_dllModule2)
                {
                    return;
                }

                spdlog::info("fgamex86.dll unloaded.");

                m_dllModule2 = nullptr;
                m_dllModule2Name.clear();
            }
        );

        if (!m_fgameWatcher->Start())
        {
            spdlog::error(
                "DllNotificationWatcher failed. NTSTATUS=0x{:08X}, Win32Error={}.",
                static_cast<unsigned long>(m_fgameWatcher->LastNtStatus()),
                m_fgameWatcher->LastWin32Error()
            );

            m_fgameWatcher.reset();
            return;
        }
    }

private:
    static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
    static constexpr float m_originalCameraFOV = 90.0f;

    SafetyHookMid m_resolutionHook{};

    HMODULE m_dllModule2 = nullptr;
    std::string m_dllModule2Name = "";

    uintptr_t m_resolutionWidthAddress = 0;
    uintptr_t m_resolutionHeightAddress = 0;

    std::unique_ptr<DllNotificationWatcher> m_fgameWatcher;

    inline static AliceFix* s_instance_ = nullptr;

    void WriteFOV()
    {
        m_newCameraFOV = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV, m_aspectRatioScale);

        auto GameplayCameraFOVScanResult = Memory::PatternScan(m_dllModule2, "C7 45 ?? ?? ?? ?? ?? EB ?? D9 45");
        if (GameplayCameraFOVScanResult)
        {
            spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), GameplayCameraFOVScanResult - reinterpret_cast<std::uint8_t*>(m_dllModule2));

            Memory::Write(GameplayCameraFOVScanResult + 3, m_newCameraFOV * m_fovFactor);
        }
        else
        {
            spdlog::error("Failed to locate gameplay FOV memory address.");
        }

        auto CutscenesCameraFOVScanResult = Memory::PatternScan(m_dllModule2, "B9 ?? ?? ?? ?? B8 ?? ?? ?? ?? C6 44 24");
        if (CutscenesCameraFOVScanResult)
        {
            spdlog::info("Cutscene Camera FOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), CutscenesCameraFOVScanResult - reinterpret_cast<std::uint8_t*>(m_dllModule2));

            Memory::Write(CutscenesCameraFOVScanResult + 1, m_newCameraFOV);
        }
        else
        {
            spdlog::error("Failed to locate cutscene camera FOV instruction memory address.");
        }
    }
};

static std::unique_ptr<AliceFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);

        g_fix = std::make_unique<AliceFix>(hModule);
        g_fix->Start();

        break;
    }

    case DLL_PROCESS_DETACH:
    {
        if (g_fix)
        {
            g_fix->Shutdown();
            g_fix.reset();
        }

        break;
    }

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    default:
        break;
    }

    return TRUE;
}