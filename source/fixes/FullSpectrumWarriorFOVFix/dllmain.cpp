#include "..\..\common\FixBase.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

class FullSpectrumWarriorFix final : public FixBase
{
public:
    explicit FullSpectrumWarriorFix(HMODULE selfModule) : FixBase(selfModule)
    {
        s_instance_ = this;
    }

    ~FullSpectrumWarriorFix() override
    {
        StopWatchdog();

        if (s_instance_ == this)
        {
            s_instance_ = nullptr;
        }
    }

protected:
    const char* FixName() const override
    {
        return "FullSpectrumWarriorFOVFix";
    }

    const char* FixVersion() const override
    {
        return "1.2";
    }

    const char* TargetName() const override
    {
        return "Full Spectrum Warrior";
    }

    InitMode GetInitMode() const override
    {
        return InitMode::Direct;
        // return InitMode::WorkerThread;
        // return InitMode::ExportedOnly;
    }

    bool IsCompatibleExecutable(const std::string& exeName) const override
    {
        return Util::stringcmp_caseless(exeName, "Launcher.exe");
    }

    void ParseFixConfig(inipp::Ini<char>& ini) override
    {
        inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
        spdlog_confparse(m_fovFactor);
    }

    void ApplyFix() override
    {
        m_dllModule2 = Memory::GetHandle("FSW.dll");
        m_dllModule2Name = Memory::GetModuleName(m_dllModule2);

        auto ResolutionScanResult = Memory::PatternScan(m_dllModule2, "8B 48 ?? 8B 40 ?? 52");
        if (ResolutionScanResult)
        {
            spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_dllModule2Name.c_str(), ResolutionScanResult - (uint8_t*)m_dllModule2);

            m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
            {
                    const int& iCurrentWidth = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0xD058);
                    const int& iCurrentHeight = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0xD05C);
                    s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
                    s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
            });
        }
        else
        {
            spdlog::error("Failed to locate resolution instructions scan memory address.");
            return;
        }

        InstallAspectRatioHook();
        InstallCameraFOVHook();

        StartWatchdog();
    }

private:
    static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

    HMODULE m_dllModule2 = nullptr;
    std::string m_dllModule2Name = "";

    std::uint8_t* m_aspectRatioAddress = nullptr;
    std::uint8_t* m_cameraFOVAddress = nullptr;

    SafetyHookMid m_resolutionHook{};
    SafetyHookMid m_aspectRatioHook{};
    SafetyHookMid m_cameraFOVHook{};

    float m_newCameraFOV = 0.0f;

    std::atomic_bool m_watchdogRunning = false;
    std::thread m_watchdogThread;
    std::mutex m_hookMutex;

    inline static FullSpectrumWarriorFix* s_instance_ = nullptr;

    void InstallAspectRatioHook()
    {
        if (!m_dllModule2)
        {
            spdlog::error("Cannot install aspect ratio hook because FSW.dll is not loaded.");
            return;
        }

        auto aspectRatioScanResult = Memory::PatternScan(m_dllModule2, "D9 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC C7 01");

        if (!aspectRatioScanResult)
        {
            spdlog::error("Failed to locate aspect ratio instruction memory address.");
            return;
        }

        m_aspectRatioAddress = aspectRatioScanResult;

        spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), m_aspectRatioAddress - reinterpret_cast<std::uint8_t*>(m_dllModule2));

        m_aspectRatioHook = {};

        Memory::WriteNOPs(m_aspectRatioAddress, 6);

        m_aspectRatioHook = safetyhook::create_mid(m_aspectRatioAddress, [](SafetyHookContext& ctx)
        {
            if (s_instance_)
            {
                FPU::FLD(s_instance_->m_newAspectRatio);
            }
        });

        spdlog::info("Aspect ratio hook installed.");
    }

    void InstallCameraFOVHook()
    {
        if (!m_dllModule2)
        {
            spdlog::error("Cannot install camera FOV hook because FSW.dll is not loaded.");
            return;
        }

        auto cameraFOVScanResult = Memory::PatternScan(m_dllModule2, "D9 44 24 ?? D9 E1 D9 54 24 ?? D9 05 ?? ?? ?? ?? DF F1 DD D8 77");

        if (!cameraFOVScanResult)
        {
            spdlog::error("Failed to locate camera FOV instruction memory address.");
            return;
        }

        m_cameraFOVAddress = cameraFOVScanResult;

        spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), m_cameraFOVAddress - reinterpret_cast<std::uint8_t*>(m_dllModule2));

        m_cameraFOVHook = {};

        Memory::WriteNOPs(m_cameraFOVAddress, 4);

        m_cameraFOVHook = safetyhook::create_mid(m_cameraFOVAddress, [](SafetyHookContext& ctx)
        {
            if (s_instance_)
            {
               s_instance_->CameraFOVMidHook(ctx);
            }
        });

        spdlog::info("Camera FOV hook installed.");
    }

    bool LooksHooked(const std::uint8_t* address) const
    {
        if (!address)
        {
            return false;
        }

        return address[0] == 0xE9;
    }

    void CheckModuleReload()
    {
        HMODULE currentModule = Memory::GetHandle("FSW.dll");

        if (!currentModule)
        {
            return;
        }

        if (currentModule != m_dllModule2)
        {
            spdlog::warn("FSW.dll appears to have been reloaded. Reapplying hooks.");

            m_dllModule2 = currentModule;
            m_dllModule2Name = Memory::GetModuleName(m_dllModule2);

            m_aspectRatioAddress = nullptr;
            m_cameraFOVAddress = nullptr;

            m_aspectRatioHook = {};
            m_cameraFOVHook = {};

            InstallAspectRatioHook();
            InstallCameraFOVHook();
        }
    }

    void StartWatchdog()
    {
        if (m_watchdogRunning)
        {
            return;
        }

        m_watchdogRunning = true;

        m_watchdogThread = std::thread([this]()
        {
                while (m_watchdogRunning)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

                    if (!m_watchdogRunning)
                    {
                        break;
                    }

                    std::lock_guard<std::mutex> lock(m_hookMutex);

                    CheckModuleReload();

                    if (!m_dllModule2)
                    {
                        continue;
                    }

                    if (m_aspectRatioAddress && !LooksHooked(m_aspectRatioAddress))
                    {
                        spdlog::warn("Aspect ratio hook appears to have been reverted. Reinstalling.");
                        InstallAspectRatioHook();
                    }

                    if (m_cameraFOVAddress && !LooksHooked(m_cameraFOVAddress))
                    {
                        spdlog::warn("Camera FOV hook appears to have been reverted. Reinstalling.");
                        InstallCameraFOVHook();
                    }
                }
        });
    }

    void StopWatchdog()
    {
        m_watchdogRunning = false;

        if (m_watchdogThread.joinable())
        {
            m_watchdogThread.join();
        }
    }

    void CameraFOVMidHook(SafetyHookContext& ctx)
    {
        float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x4);

        if (fCurrentCameraFOV != m_newCameraFOV)
        {
            m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;
        }

        FPU::FLD(m_newCameraFOV);
    }
};

static std::unique_ptr<FullSpectrumWarriorFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);

        g_fix = std::make_unique<FullSpectrumWarriorFix>(hModule);
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