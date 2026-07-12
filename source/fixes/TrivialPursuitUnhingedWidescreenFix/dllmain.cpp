#include "..\..\common\FixBase.hpp"

class TrivialPursuitUnhingedFix final : public FixBase
{
public:
    explicit TrivialPursuitUnhingedFix(HMODULE selfModule) : FixBase(selfModule)
    {
        s_instance_ = this;
    }

    ~TrivialPursuitUnhingedFix() override
    {
        if (s_instance_ == this)
        {
            s_instance_ = nullptr;
        }
    }

protected:
    const char* FixName() const override
    {
        return "TrivialPursuitUnhingedWidescreenFix";
    }

    const char* FixVersion() const override
    {
        return "1.1";
    }

    const char* TargetName() const override
    {
        return "Trivial Pursuit: Unhinged";
    }

    InitMode GetInitMode() const override
    {
        return InitMode::Direct;
        // return InitMode::WorkerThread;
        // return InitMode::ExportedOnly;
    }

    bool IsCompatibleExecutable(const std::string& exeName) const override
    {
        return Util::stringcmp_caseless(exeName, "TrivialPursuitPC.exe") ||
        Util::stringcmp_caseless(exeName, "TPPCFrench.EXE") ||
		Util::stringcmp_caseless(exeName, "TPPCGerman.EXE") ||
		Util::stringcmp_caseless(exeName, "TTPCItalian.EXE") ||
		Util::stringcmp_caseless(exeName, "TPPCSpanish.EXE") ||
		Util::stringcmp_caseless(exeName, "TPPCUK.EXE");
    }

    void ParseFixConfig(inipp::Ini<char>& ini) override
    {
        inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
        inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
        spdlog_confparse(m_runMultipleInstances);
        spdlog_confparse(m_skipIntroVideos);
    }

    void ApplyFix() override
    {
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "75 ?? 3B C6 75 ?? 89 59", "50 57 52 53 e8", "8b 54 24 ?? 89 81 ?? ?? ?? ?? 8b 44 24");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Video Setup Dialog Check Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[VidSetup] - (std::uint8_t*)ExeModule());
			spdlog::info("Video Setup Dialog Default Resolution Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[VidSetupDefaultRes] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

            Memory::PatchBytes(ResolutionScansResult[VidSetup], "\xEB");

            m_vidSetupDefaultResHook = safetyhook::create_mid(ResolutionScansResult[VidSetupDefaultRes], [](SafetyHookContext& ctx)
            {
                s_instance_->m_vidSetupDefaultWidth = Screen::GetDesktopResolutionWidth();
                s_instance_->m_vidSetupDefaultHeight = Screen::GetDesktopResolutionHeight();
                ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_vidSetupDefaultWidth);
                ctx.edi = std::bit_cast<uintptr_t>(s_instance_->m_vidSetupDefaultHeight);
                s_instance_->m_vidSetupDefaultResHook.disable();
            });

            m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
            {
                s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x2C);
                s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x30);
                s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
                s_instance_->m_resolutionHook.disable();
            });
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 06 ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 89 7E");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());            

			Memory::WriteNOPs(AspectRatioScanResult, 6);

            m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
            {
                *reinterpret_cast<float*>(ctx.esi) = s_instance_->m_newAspectRatio;
            });
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

        if (m_runMultipleInstances == true)
        {
            auto RunMultipleInstancesScanResult = Memory::PatternScan(ExeModule(), "74 ?? B8 ?? ?? ?? ?? C2 ?? ?? 8B 44 24");
            if (RunMultipleInstancesScanResult)
            {
                spdlog::info("Run Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesScanResult - (std::uint8_t*)ExeModule());

                Memory::PatchBytes(RunMultipleInstancesScanResult, "\xEB");
            }
            else
            {
                spdlog::error("Failed to locate run multiple instances check instruction memory address.");
                return;
            }
        }

        if (m_skipIntroVideos == true)
        {
            auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? A1 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ?? 66 C7 44 24");
            if (SkipIntroVideosScanResult)
            {
                spdlog::info("Skip Intro Videos Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

                Memory::PatchBytes(SkipIntroVideosScanResult, "\xE9\x03\x01\x00\x00\x90");
            }
            else
            {
                spdlog::error("Failed to locate skip intro videos instruction memory address.");
                return;
            }
        }
    }

private:
    bool m_runMultipleInstances = false;
    bool m_skipIntroVideos = false;

    int m_vidSetupDefaultWidth = 0;
    int m_vidSetupDefaultHeight = 0;

    SafetyHookMid m_vidSetupDefaultResHook{};
    SafetyHookMid m_resolutionHook{};    
    SafetyHookMid m_aspectRatioHook{};

    enum ResolutionInstructionsScansIndices
    {
        VidSetup,
        VidSetupDefaultRes,
        WidthHeight
    };    

    inline static TrivialPursuitUnhingedFix* s_instance_ = nullptr;
};

static std::unique_ptr<TrivialPursuitUnhingedFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);
        g_fix = std::make_unique<TrivialPursuitUnhingedFix>(hModule);
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