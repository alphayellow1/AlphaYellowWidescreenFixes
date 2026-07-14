#include "..\..\common\FixBase.hpp"

class TheMummyFix final : public FixBase
{
public:
	explicit TheMummyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TheMummyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheMummyFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.6";
	}

	const char* TargetName() const override
	{
		return "The Mummy";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "MummyPC.exe");
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
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 90 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 89 15");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionWidthOffset = Memory::GetPointerFromAddress(ResolutionScanResult + 8, Memory::PointerMode::Absolute);
			m_resolutionHeightOffset = Memory::GetPointerFromAddress(ResolutionScanResult + 2, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->hitCount++;

				if (s_instance_->hitCount == 2)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.eax + s_instance_->m_resolutionWidthOffset);
					int& iCurrentHeight = Memory::ReadMem(ctx.eax + s_instance_->m_resolutionHeightOffset);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->WriteStaticFOVs();
					s_instance_->m_resolutionHook.disable();
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 89 44 24 ?? 8B 42");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? A3", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 8B CE");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "6A ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68",
			"6A ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 59 C3 90 A1", "E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F0", "68 ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? E8");
			if (Memory::AreAllSignaturesValid(SkipIntroVideosScansResult) == true)
			{
				spdlog::info("Skip Intro Videos: startup logo FMV sequence found at {:s}+{:x}", ExeName(), SkipIntroVideosScansResult[StartupLogoSequence] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos: ED_book_hi.bik playback found at {:s}+{:x}", ExeName(), SkipIntroVideosScansResult[BookIntro] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos: license1 image loader call found at {:s}+{:x}", ExeName(), SkipIntroVideosScansResult[LicenseImageLoader] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos: license image display duration found at {:s}+{:x}", ExeName(), SkipIntroVideosScansResult[LicenseDisplayDelay] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScansResult[StartupLogoSequence], "\xEB\x31");
				Memory::PatchBytes(SkipIntroVideosScansResult[BookIntro], "\xEB\x0F");
				Memory::WriteNOPs(SkipIntroVideosScansResult[LicenseImageLoader], 5);
				Memory::Write(SkipIntroVideosScansResult[LicenseDisplayDelay] + 1, m_skippedDisplayDuration);
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr int m_iOriginalCutscenesFOV = 1024;
	static constexpr int m_iOriginalGameplayFOV = 683;
	static constexpr float m_skippedDisplayDuration = 0.000001f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};

	uintptr_t m_resolutionWidthOffset = 0;
	uintptr_t m_resolutionHeightOffset = 0;

	bool m_skipIntroVideos = false;

	std::vector<std::uint8_t*> CameraFOVScansResult;

	float m_fOriginalCutscenesFOV = 0.0f;
	float m_fNewCutscenesFOV = 0.0f;
	int m_iNewCutscenesFOV = 0;
	float m_fOriginalGameplayFOV = 0.0f;
	float m_fNewGameplayFOV = 0.0f;
	int m_iNewGameplayFOV = 0;

	int hitCount = 0;

	enum CameraFOVInstructionsIndex
	{
		Cutscenes,
		Gameplay
	};

	enum SkipIntroLogosIndex
	{
		StartupLogoSequence,
		BookIntro,
		LicenseImageLoader,
		LicenseDisplayDelay
	};

	void WriteStaticFOVs()
	{
		m_fOriginalCutscenesFOV = (float)m_iOriginalCutscenesFOV * 360.0f / 4096.0f;
		m_fNewCutscenesFOV = Maths::CalculateNewFOV_DegBased(m_fOriginalCutscenesFOV, m_aspectRatioScale);
		m_iNewCutscenesFOV = (int)(m_fNewCutscenesFOV * 4096.0f / 360.0f);

		m_fOriginalGameplayFOV = (float)m_iOriginalGameplayFOV * 360.0f / 4096.0f;
		m_fNewGameplayFOV = Maths::CalculateNewFOV_DegBased(m_fOriginalGameplayFOV, m_aspectRatioScale) * m_fovFactor;
		m_iNewGameplayFOV = (int)(m_fNewGameplayFOV * 4096.0f / 360.0f);

		Memory::Write(CameraFOVScansResult[Cutscenes] + 1, m_iNewCutscenesFOV);
		Memory::Write(CameraFOVScansResult[Gameplay] + 1, m_iNewGameplayFOV);
	}

	inline static TheMummyFix* s_instance_ = nullptr;
};

static std::unique_ptr<TheMummyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<TheMummyFix>(hModule);
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