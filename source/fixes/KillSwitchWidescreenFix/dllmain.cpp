#include "..\..\common\FixBase.hpp"

class KillSwitchFix final : public FixBase
{
public:
    explicit KillSwitchFix(HMODULE selfModule) : FixBase(selfModule)
    {
        s_instance_ = this;
    }

    ~KillSwitchFix() override
    {
        if (s_instance_ == this)
        {
            s_instance_ = nullptr;
        }
    }

protected:
    const char* FixName() const override
    {
        return "KillSwitchWidescreenFix";
    }

    const char* FixVersion() const override
    {
        return "1.5";
    }

    const char* TargetName() const override
    {
        return "Kill Switch";
    }

    InitMode GetInitMode() const override
    {
        // return InitMode::Direct;
        return InitMode::WorkerThread;
        // return InitMode::ExportedOnly;
    }

    bool IsCompatibleExecutable(const std::string& exeName) const override
    {
        return Util::stringcmp_caseless(exeName, "killswitchsetup.exe") ||
        Util::stringcmp_caseless(exeName, "KILLSWITCH.EXE");
    }

    void ParseFixConfig(inipp::Ini<char>& ini) override
    {
        inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
        inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
        spdlog_confparse(m_fovFactor);
        spdlog_confparse(m_skipIntroVideos);
    }

    void ApplyFix() override
    {
        if (Util::stringcmp_caseless(ExeName(), "killswitchsetup.exe"))
        {
            InstallResolutionHooks();
        }

        if (Util::stringcmp_caseless(ExeName(), "KILLSWITCH.EXE"))
        {
            auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 0A 89 0D ?? ?? ?? ?? 8B 4A ?? 89 0D ?? ?? ?? ?? 8B 4A");
            if (ResolutionScanResult)
            {
                spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

                m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
                {
                    int& iCurrentWidth = Memory::ReadMem(ctx.edx);
                    int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
                    s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
                    s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
                });
            }
            else
            {
                spdlog::info("Failed to locate the resolution scan memory address.");
                return;
            }

            auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 54 24 ?? 52 83 C6", "D9 45 ?? D9 57", "B8 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? BA");
            if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
            {
                spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes] - (std::uint8_t*)ExeModule());
                spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay1] - (std::uint8_t*)ExeModule());
                spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay2] - (std::uint8_t*)ExeModule());

                Memory::WriteNOPs(CameraFOVScansResult[Cutscenes], 4);

                m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes], [](SafetyHookContext& ctx)
                {
                    float& fCurrentCutscenesFOV = Memory::ReadMem(ctx.esp + 0x48);
                    s_instance_->m_newCutscenesFOV = Maths::CalculateNewFOV_RadBased(fCurrentCutscenesFOV, s_instance_->m_aspectRatioScale);
                    ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newCutscenesFOV);
                });

                Memory::WriteNOPs(CameraFOVScansResult[Gameplay1], 3);

                m_gameplayFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay1], [](SafetyHookContext& ctx)
                {
                    float& fCurrentGameplayFOV1 = Memory::ReadMem(ctx.ebp + 0x3C);
                    s_instance_->m_newGameplayFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentGameplayFOV1, s_instance_->m_aspectRatioScale);
                    FPU::FLD(s_instance_->m_newGameplayFOV1);
                });

                m_newGameplayFOV2 = 0.7853981853f * m_fovFactor;

                Memory::Write(CameraFOVScansResult[Gameplay2] + 1, m_newGameplayFOV2);
            }

            if (m_skipIntroVideos == true)
            {
                auto SkipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 8B CE E8 ?? ?? ?? ?? B8", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 8B CE E8 ?? ?? ?? ?? 68",
                "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 8B CE E8 ?? ?? ?? ?? BB", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 8B CE E8 ?? ?? ?? ?? 8B 15", "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 8B CE E8 ?? ?? ?? ?? 89 5E",
                "B8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 55 8B CE E8 ?? ?? ?? ?? C7 46", "74 ?? 8B 15 ?? ?? ?? ?? 53 6A", "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 88 86",
                "74 ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 89 7E");
                if (Memory::AreAllSignaturesValid(SkipIntroVideosScansResult) == true)
                {
                    spdlog::info("Skip Intro Videos Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids1] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids2] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids3] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids4] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids5] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids6] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids7] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids8] - (std::uint8_t*)ExeModule());
                    spdlog::info("Skip Intro Videos Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipIntroVids9] - (std::uint8_t*)ExeModule());

                    Memory::PatchBytes(SkipIntroVideosScansResult, SkipIntroVids1, SkipIntroVids6, 1, "\x00");
                    Memory::PatchBytes(SkipIntroVideosScansResult[SkipIntroVids7], "\xEB");
                    Memory::Write(SkipIntroVideosScansResult[SkipIntroVids8] + 6, 0.0f);
                    Memory::PatchBytes(SkipIntroVideosScansResult[SkipIntroVids9], "\xEB");
                }                
            }            
        }
    }

private:
    static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

    bool m_skipIntroVideos = false;

    enum SkipIntroVideosInstructionsIndex
    {
        SkipIntroVids1,
        SkipIntroVids2,
        SkipIntroVids3,
        SkipIntroVids4,
        SkipIntroVids5,
        SkipIntroVids6,
        SkipIntroVids7,
        SkipIntroVids8,
        SkipIntroVids9
    };

    struct Resolution
    {
        int width;
        int height;
    };

    float m_newAspectRatio = 4.0f / 3.0f;
    float m_aspectRatioScale = 1.0f;

    float m_newCutscenesFOV = 0.0f;
    float m_newGameplayFOV1 = 0.0f;
    float m_newGameplayFOV2 = 0.0f;

    enum CameraFOVInstructionsIndices
    {
        Cutscenes,
        Gameplay1,
        Gameplay2
    };

    enum class ResolutionControlType
    {
        None,
        ComboBox,
        ListBox
    };

    uintptr_t g_moduleBase = 0;

    std::vector<Resolution> g_systemResolutions;

    SafetyHookMid m_resolutionHook = {};
    SafetyHookMid m_cutscenesFOVHook = {};
    SafetyHookMid m_gameplayFOV1Hook = {};
    SafetyHookMid m_resolutionListHook = {};
    SafetyHookMid m_resolutionSaveHook = {};

    HWND g_resolutionControl = nullptr;
    HWND g_resolutionDialog = nullptr;
    WNDPROC g_originalDialogProc = nullptr;

    ResolutionControlType g_resolutionControlType = ResolutionControlType::None;

    int g_selectedResolutionIndex = 0;
    Resolution g_selectedResolution = { 640, 480 };
    bool g_hasSelectedResolution = false;

    static bool SameResolution(const Resolution& a, const Resolution& b)
    {
        return a.width == b.width && a.height == b.height;
    }

    static std::string FormatResolutionA(const Resolution& res)
    {
        return std::to_string(res.width) + "x" + std::to_string(res.height);
    }

    void SetSelectedResolutionIndex(int index)
    {
        if (index < 0 || index >= static_cast<int>(g_systemResolutions.size()))
        {
            return;
        }

        g_selectedResolutionIndex = index;
        g_selectedResolution = g_systemResolutions[index];
        g_hasSelectedResolution = true;
    }

    void BuildSystemResolutionList()
    {
        g_systemResolutions.clear();

        DEVMODEA dm{};
        dm.dmSize = sizeof(dm);

        for (DWORD i = 0; EnumDisplaySettingsA(nullptr, i, &dm); ++i)
        {
            if (dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480)
            {
                continue;
            }

            Resolution res
            {
                static_cast<int>(dm.dmPelsWidth),
                static_cast<int>(dm.dmPelsHeight)
            };

            auto exists = std::find_if(g_systemResolutions.begin(), g_systemResolutions.end(),[&](const Resolution& other)
            {
                return SameResolution(res, other);
            });

            if (exists == g_systemResolutions.end())
            {
                g_systemResolutions.push_back(res);
            }
        }

        std::sort(g_systemResolutions.begin(), g_systemResolutions.end(), [](const Resolution& a, const Resolution& b)
        {
            if (a.width != b.width)
            {
                return a.width < b.width;
            }

            return a.height < b.height;
        });
    }

    bool ClassNameIs(HWND hwnd, const char* expected)
    {
        char className[64]{};
        GetClassNameA(hwnd, className, sizeof(className));
        return _stricmp(className, expected) == 0;
    }

    int GetComboCount(HWND hwnd)
    {
        return static_cast<int>(SendMessageA(hwnd, CB_GETCOUNT, 0, 0));
    }

    int GetListCount(HWND hwnd)
    {
        return static_cast<int>(SendMessageA(hwnd, LB_GETCOUNT, 0, 0));
    }

    std::string GetComboText(HWND hwnd, int index)
    {
        LRESULT len = SendMessageA(hwnd, CB_GETLBTEXTLEN, index, 0);

        if (len <= 0 || len > 512)
        {
            return {};
        }

        std::string text;
        text.resize(static_cast<size_t>(len) + 1);

        SendMessageA(hwnd, CB_GETLBTEXT, index, reinterpret_cast<LPARAM>(text.data()));

        text.resize(static_cast<size_t>(len));
        return text;
    }

    std::string GetListText(HWND hwnd, int index)
    {
        LRESULT len = SendMessageA(hwnd, LB_GETTEXTLEN, index, 0);

        if (len <= 0 || len > 512)
        {
            return {};
        }

        std::string text;
        text.resize(static_cast<size_t>(len) + 1);

        SendMessageA(hwnd, LB_GETTEXT, index, reinterpret_cast<LPARAM>(text.data()));

        text.resize(static_cast<size_t>(len));
        return text;
    }

    bool TextLooksLikeResolution(const std::string& text)
    {
        return text.find("640") != std::string::npos ||
            text.find("800") != std::string::npos ||
            text.find("1024") != std::string::npos ||
            text.find("480") != std::string::npos ||
            text.find("600") != std::string::npos ||
            text.find("768") != std::string::npos;
    }

    bool LooksLikeResolutionCombo(HWND hwnd)
    {
        int count = GetComboCount(hwnd);

        if (count <= 0 || count > 64)
        {
            return false;
        }

        int matches = 0;

        for (int i = 0; i < count; ++i)
        {
            std::string text = GetComboText(hwnd, i);

            if (TextLooksLikeResolution(text))
            {
                ++matches;
            }
        }

        return matches > 0;
    }

    bool LooksLikeResolutionList(HWND hwnd)
    {
        int count = GetListCount(hwnd);

        if (count <= 0 || count > 64)
        {
            return false;
        }

        int matches = 0;

        for (int i = 0; i < count; ++i)
        {
            std::string text = GetListText(hwnd, i);

            if (TextLooksLikeResolution(text))
            {
                ++matches;
            }
        }

        return matches > 0;
    }

    int FindCurrentResolutionIndex()
    {
        int currentWidth = *reinterpret_cast<int*>(g_moduleBase + 0x14F934);
        int currentHeight = *reinterpret_cast<int*>(g_moduleBase + 0x14F938);

        for (int i = 0; i < static_cast<int>(g_systemResolutions.size()); ++i)
        {
            const Resolution& res = g_systemResolutions[i];

            if (res.width == currentWidth && res.height == currentHeight)
            {
                return i;
            }
        }

        return 0;
    }

    void UpdateSelectedResolutionFromControl()
    {
        if (!g_resolutionControl || !IsWindow(g_resolutionControl))
        {
            return;
        }

        if (g_resolutionControlType == ResolutionControlType::ComboBox)
        {
            LRESULT selected = SendMessageA(g_resolutionControl, CB_GETCURSEL, 0, 0);

            if (selected >= 0)
            {
                LRESULT itemData = SendMessageA(g_resolutionControl, CB_GETITEMDATA, selected, 0);

                if (itemData >= 0 && itemData < static_cast<LRESULT>(g_systemResolutions.size()))
                {
                    SetSelectedResolutionIndex(static_cast<int>(itemData));
                }
            }
        }
        else if (g_resolutionControlType == ResolutionControlType::ListBox)
        {
            LRESULT selected = SendMessageA(g_resolutionControl, LB_GETCURSEL, 0, 0);

            if (selected >= 0)
            {
                LRESULT itemData = SendMessageA(g_resolutionControl, LB_GETITEMDATA, selected, 0);

                if (itemData >= 0 && itemData < static_cast<LRESULT>(g_systemResolutions.size()))
                {
                    SetSelectedResolutionIndex(static_cast<int>(itemData));
                }
            }
        }
    }

    void FillResolutionCombo(HWND hwnd)
    {
        SendMessageA(hwnd, CB_RESETCONTENT, 0, 0);

        for (int i = 0; i < static_cast<int>(g_systemResolutions.size()); ++i)
        {
            const Resolution& res = g_systemResolutions[i];
            std::string text = FormatResolutionA(res);

            LRESULT itemIndex = SendMessageA(hwnd, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));

            if (itemIndex >= 0)
            {
                SendMessageA(hwnd, CB_SETITEMDATA, itemIndex, static_cast<LPARAM>(i));
            }
        }

        int currentIndex = FindCurrentResolutionIndex();
        bool selectedCurrent = false;

        int count = static_cast<int>(SendMessageA(hwnd, CB_GETCOUNT, 0, 0));

        for (int visibleIndex = 0; visibleIndex < count; ++visibleIndex)
        {
            LRESULT itemData = SendMessageA(hwnd, CB_GETITEMDATA, visibleIndex, 0);

            if (itemData == currentIndex)
            {
                SendMessageA(hwnd, CB_SETCURSEL, visibleIndex, 0);
                SetSelectedResolutionIndex(currentIndex);
                selectedCurrent = true;
                break;
            }
        }

        if (!selectedCurrent && count > 0)
        {
            SendMessageA(hwnd, CB_SETCURSEL, 0, 0);

            LRESULT itemData = SendMessageA(hwnd, CB_GETITEMDATA, 0, 0);

            if (itemData >= 0 && itemData < static_cast<LRESULT>(g_systemResolutions.size()))
            {
                SetSelectedResolutionIndex(static_cast<int>(itemData));
            }
        }

        g_resolutionControl = hwnd;
        g_resolutionControlType = ResolutionControlType::ComboBox;

        UpdateSelectedResolutionFromControl();
    }

    void FillResolutionList(HWND hwnd)
    {
        SendMessageA(hwnd, LB_RESETCONTENT, 0, 0);

        for (int i = 0; i < static_cast<int>(g_systemResolutions.size()); ++i)
        {
            const Resolution& res = g_systemResolutions[i];
            std::string text = FormatResolutionA(res);

            LRESULT itemIndex = SendMessageA(hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));

            if (itemIndex >= 0)
            {
                SendMessageA(hwnd, LB_SETITEMDATA, itemIndex, static_cast<LPARAM>(i));
            }
        }

        int currentIndex = FindCurrentResolutionIndex();
        bool selectedCurrent = false;

        int count = static_cast<int>(SendMessageA(hwnd, LB_GETCOUNT, 0, 0));

        for (int visibleIndex = 0; visibleIndex < count; ++visibleIndex)
        {
            LRESULT itemData = SendMessageA(hwnd, LB_GETITEMDATA, visibleIndex, 0);

            if (itemData == currentIndex)
            {
                SendMessageA(hwnd, LB_SETCURSEL, visibleIndex, 0);
                SetSelectedResolutionIndex(currentIndex);
                selectedCurrent = true;
                break;
            }
        }

        if (!selectedCurrent && count > 0)
        {
            SendMessageA(hwnd, LB_SETCURSEL, 0, 0);

            LRESULT itemData = SendMessageA(hwnd, LB_GETITEMDATA, 0, 0);

            if (itemData >= 0 && itemData < static_cast<LRESULT>(g_systemResolutions.size()))
            {
                SetSelectedResolutionIndex(static_cast<int>(itemData));
            }
        }

        g_resolutionControl = hwnd;
        g_resolutionControlType = ResolutionControlType::ListBox;

        UpdateSelectedResolutionFromControl();
    }

    LRESULT ResolutionDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            case WM_COMMAND:
            {
                HWND control = reinterpret_cast<HWND>(lParam);
                WORD controlId = LOWORD(wParam);
                WORD notification = HIWORD(wParam);

                if (control == g_resolutionControl)
                {
                    if (notification == CBN_SELCHANGE || notification == CBN_SELENDOK || notification == CBN_CLOSEUP || notification == LBN_SELCHANGE)
                    {
                        UpdateSelectedResolutionFromControl();
                    }
                }

                if (controlId == IDOK)
                {
                    UpdateSelectedResolutionFromControl();
                }

                break;
            }

            case WM_CLOSE:
            case WM_DESTROY:
                UpdateSelectedResolutionFromControl();
                break;

            case WM_NCDESTROY:
            {
                UpdateSelectedResolutionFromControl();

                WNDPROC originalProc = g_originalDialogProc;

                g_resolutionDialog = nullptr;
                g_resolutionControl = nullptr;
                g_resolutionControlType = ResolutionControlType::None;
                g_originalDialogProc = nullptr;

                if (originalProc)
                {
                    return CallWindowProcA(originalProc, hwnd, msg, wParam, lParam);
                }

                return DefWindowProcA(hwnd, msg, wParam, lParam);
            }
        }

        if (g_originalDialogProc)
        {
            return CallWindowProcA(g_originalDialogProc, hwnd, msg, wParam, lParam);
        }

        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    static LRESULT CALLBACK ResolutionDialogProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (s_instance_)
        {
            return s_instance_->ResolutionDialogProc(hwnd, msg, wParam, lParam);
        }

        return DefWindowProcA(hwnd, msg, wParam, lParam);
    }

    void SubclassResolutionDialog(HWND dialog)
    {
        if (!dialog || !IsWindow(dialog))
        {
            return;
        }

        if (g_resolutionDialog == dialog && g_originalDialogProc)
        {
            return;
        }

        g_resolutionDialog = dialog;

        g_originalDialogProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(dialog, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&KillSwitchFix::ResolutionDialogProcStatic)));

        if (!g_originalDialogProc)
        {
            spdlog::warn("Failed to subclass setup dialog.");
        }
    }

    struct ResolutionControlSearch
    {
        KillSwitchFix* self = nullptr;
        bool found = false;
    };

    BOOL EnumResolutionControls(HWND hwnd, ResolutionControlSearch* search)
    {
        if (ClassNameIs(hwnd, "ComboBox") && LooksLikeResolutionCombo(hwnd))
        {
            FillResolutionCombo(hwnd);
            search->found = true;
            return FALSE;
        }

        if (ClassNameIs(hwnd, "ListBox") && LooksLikeResolutionList(hwnd))
        {
            FillResolutionList(hwnd);
            search->found = true;
            return FALSE;
        }

        return TRUE;
    }

    static BOOL CALLBACK EnumResolutionControlsStatic(HWND hwnd, LPARAM lParam)
    {
        auto* search = reinterpret_cast<ResolutionControlSearch*>(lParam);

        if (!search || !search->self)
        {
            return TRUE;
        }

        return search->self->EnumResolutionControls(hwnd, search);
    }

    void ReplaceResolutionListInDialog(HWND dialog)
    {
        if (!dialog || !IsWindow(dialog))
        {
            spdlog::warn("Invalid setup dialog HWND.");
            return;
        }

        ResolutionControlSearch search{};
        search.self = this;

        EnumChildWindows(dialog, &KillSwitchFix::EnumResolutionControlsStatic, reinterpret_cast<LPARAM>(&search));

        if (search.found)
        {
            SubclassResolutionDialog(dialog);
        }
    }

    void InstallResolutionHooks()
    {
        g_moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));

        BuildSystemResolutionList();

        if (g_systemResolutions.empty())
        {
            spdlog::warn("No system resolutions found. Resolution hooks will not be installed.");
            return;
        }

        m_resolutionListHook = safetyhook::create_mid(reinterpret_cast<uint8_t*>(g_moduleBase + 0xE4C2B), [](SafetyHookContext& ctx)
        {
            if (!s_instance_)
            {
                return;
            }

            HWND dialog = reinterpret_cast<HWND>(ctx.eax);

            s_instance_->ReplaceResolutionListInDialog(dialog);
        });

        m_resolutionSaveHook = safetyhook::create_mid(reinterpret_cast<uint8_t*>(g_moduleBase + 0xB46C), [](SafetyHookContext& ctx)
        {
            if (!s_instance_)
            {
                return;
            }

            s_instance_->UpdateSelectedResolutionFromControl();

            int originalSelectedIndex = *reinterpret_cast<int*>(ctx.esp + 0xC0);

            if (!s_instance_->g_hasSelectedResolution)
            {
                spdlog::warn("No tracked resolution. Letting original code run.");
                return;
            }

            const Resolution& res = s_instance_->g_selectedResolution;

            *reinterpret_cast<int*>(s_instance_->g_moduleBase + 0x14F934) = res.width;
            *reinterpret_cast<int*>(s_instance_->g_moduleBase + 0x14F938) = res.height;

            ctx.eip = s_instance_->g_moduleBase + 0xB4B9;
        });
    }

    inline static KillSwitchFix* s_instance_ = nullptr;
};

static std::unique_ptr<KillSwitchFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);
        g_fix = std::make_unique<KillSwitchFix>(hModule);
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