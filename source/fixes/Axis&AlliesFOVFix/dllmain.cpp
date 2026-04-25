#include "..\..\common\FixBase.hpp"

class AxisAlliesFix final : public FixBase
{
public:
    explicit AxisAlliesFix(HMODULE selfModule) : FixBase(selfModule)
    {
        s_instance_ = this;
    }

    ~AxisAlliesFix() override
    {
        if (s_instance_ == this)
        {
            s_instance_ = nullptr;
        }
    }

protected:
    const char* FixName() const override
    {
        return "Axis&AlliesFOVFix";
    }

    const char* FixVersion() const override
    {
        return "1.1";
    }

    const char* TargetName() const override
    {
        return "Axis & Allies";
    }

    InitMode GetInitMode() const override
    {
        // return InitMode::Direct;
        return InitMode::WorkerThread;
        // return InitMode::ExportedOnly;
    }

    bool IsCompatibleExecutable(const std::string& exeName) const override
    {
        return Util::stringcmp_caseless(exeName, "AA.exe");
    }

    void ParseFixConfig(inipp::Ini<char>& ini) override
    {
        inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

        spdlog_confparse(m_fovFactor);
    }

    void ApplyFix() override
    {
        auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 45 ?? 89 55 ?? 89 46");
		if (!ResolutionScanResult)
        {
            spdlog::info("Cannot locate the resolution instruction memory address.");
            return;
        }

        m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
        {
                if (!s_instance_)
                {
                    return;
                }

                int& iCurrentWidth = Memory::ReadMem(ctx.ebp + 0x10);

                int& iCurrentHeight = Memory::ReadMem(ctx.ebp + 0x14);

                s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

                s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
        });

        auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 8E ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 8E ?? ?? ?? ?? D8 8E",
        "D8 8E ?? ?? ?? ?? D9 5C 24 ?? D9 C0 D8 8E ?? ?? ?? ?? D9 5C 24 ?? D8 8E ?? ?? ?? ?? D9 1C 24");
        if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == false)
        {
            return;
        }

        spdlog::info("Aspect Ratio Instruction 1: Address is {}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - reinterpret_cast<std::uint8_t*>(ExeModule()));
        spdlog::info("Aspect Ratio Instruction 2: Address is {}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - reinterpret_cast<std::uint8_t*>(ExeModule()));

        Memory::WriteNOPs(AspectRatioScansResult, AR1, AR2, 0, 6);

        m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], AspectRatioMidHook);
        m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], AspectRatioMidHook);

        std::uint8_t* CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 86 ?? ?? ?? ?? A1");
        if (!CameraFOVScanResult)
        {
            spdlog::info("Cannot locate the camera FOV instruction memory address.");
            return;
        }

        spdlog::info("Camera FOV Instruction: Address is {}+{:x}", ExeName().c_str(), CameraFOVScanResult - reinterpret_cast<std::uint8_t*>(ExeModule()));

        Memory::WriteNOPs(CameraFOVScanResult, 6);

        m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
        {
            if (!s_instance_)
            {
                return;
            }

            float& currentCameraFOV = Memory::ReadMem(ctx.esi + 0x218);
            s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_DegBased(currentCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
            FPU::FLD(s_instance_->m_newCameraFOV);
        });
    }

private:
    static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

    inline static AxisAlliesFix* s_instance_ = nullptr;

    SafetyHookMid m_resolutionHook{};
    SafetyHookMid m_aspectRatio1Hook{};
    SafetyHookMid m_aspectRatio2Hook{};
    SafetyHookMid m_cameraFOVHook{};

    enum AspectRatioInstructionsIndex
    {
        AR1,
        AR2
    };

    static void AspectRatioMidHook(SafetyHookContext& ctx)
    {
        if (!s_instance_)
        {
            return;
        }

        float& currentAspectRatio = Memory::ReadMem(ctx.esi + 0x230);
        s_instance_->m_newAspectRatio2 = currentAspectRatio / s_instance_->m_aspectRatioScale;
        FPU::FMUL(s_instance_->m_newAspectRatio2);
    }
};

static std::unique_ptr<AxisAlliesFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);

        g_fix = std::make_unique<AxisAlliesFix>(hModule);

        g_fix->Start();

        break;
    }

    case DLL_PROCESS_DETACH:
    {
        g_fix->Shutdown();
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