#include "..\..\common\FixBase.hpp"

class AngelsVsDevilsFix final : public FixBase
{
public:
	explicit AngelsVsDevilsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AngelsVsDevilsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AngelsVsDevilsWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3.1";
	}

	const char* TargetName() const override
	{
		return "Angels vs Devils";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Config.exe") || 
		Util::stringcmp_caseless(exeName, "AngelsvsDevils.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "MainMenuWidth", m_newMainMenuResX);
		inipp::get_value(ini.sections["Settings"], "MainMenuHeight", m_newMainMenuResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);

		FallbackToDesktopResolution(m_newMainMenuResX, m_newMainMenuResY);

		spdlog_confparse(m_newMainMenuResX);
		spdlog_confparse(m_newMainMenuResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "Config.exe"))
		{
			m_resolutionListUnlockScansResult = Memory::PatternScan(ExeModule(), "81 FF ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? EB ?? 81 FF ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? EB ?? 81 FF ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? EB ?? 81 FF ?? ?? ?? ?? 75",
			"32 C9 3D ?? ?? ?? ?? 75");
			if (Memory::AreAllSignaturesValid(m_resolutionListUnlockScansResult) == true)
			{
				spdlog::info("Resolution List Unlock 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListUnlockScansResult[ResListUnlock1] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution List Unlock 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListUnlockScansResult[ResListUnlock2] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_resolutionListUnlockScansResult[ResListUnlock1], 122);
				Memory::WriteNOPs(m_resolutionListUnlockScansResult[ResListUnlock2], 89);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "AngelsvsDevils.exe"))
		{
			m_resolutionScansResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? A3 ?? ?? ?? ?? A3", "8B 4D ?? 89 8E");
			if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
			{
				spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[MainMenu] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

				Memory::Write(m_resolutionScansResult[MainMenu] + 6, m_newMainMenuResX);
				Memory::Write(m_resolutionScansResult[MainMenu] + 16, m_newMainMenuResY);

				m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.ebp + 0x8);
					int& iCurrentHeight = Memory::ReadMem(ctx.ebp + 0xC);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->UpdateFOV();
				});
			}

			m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 6A ?? A1", "68 ?? ?? ?? ?? 6A ?? 83 EC ?? 8B CC",
			"68 ?? ?? ?? ?? 6A ?? 83 EC ?? C7 44 24 ?? ?? ?? ?? ?? 8B 44 24", "68 ?? ?? ?? ?? 6A ?? 8B 44 24");
			if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
				spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
				spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			}

			if (m_skipIntroVideos == true)
			{
				m_skipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "75 ?? 8B 0D ?? ?? ?? ?? 51 68");
				if (m_skipIntroVideosScanResult)
				{
					spdlog::info("Skip Intro Videos Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScanResult - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(m_skipIntroVideosScanResult, "\xEB");
				}
				else
				{
					spdlog::error("Failed to locate skip intro videos instruction memory address.");
					return;
				}
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV1  = 0.7853981853f;
	static constexpr float m_originalCameraFOV2 = 0.3926990926f;

	bool m_skipIntroVideos = false;
	int m_newMainMenuResX = 0;
	int m_newMainMenuResY = 0;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	std::vector<std::uint8_t*> m_resolutionListUnlockScansResult{};
	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::uint8_t* m_skipIntroVideosScanResult = nullptr;

	SafetyHookMid m_resolutionHook{};	

	enum ResolutionListUnlockIndex
	{
		ResListUnlock1,
		ResListUnlock2
	};

	enum ResolutionInstructionsIndex
	{
		MainMenu,
		ResWidthHeight
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4
	};

	void UpdateFOV()
	{
		m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(m_originalCameraFOV1, m_aspectRatioScale);
		m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(m_originalCameraFOV2, m_aspectRatioScale);

		Memory::Write(m_cameraFOVScansResult[FOV1] + 1, m_newCameraFOV1);
		Memory::Write(m_cameraFOVScansResult[FOV2] + 1, m_newCameraFOV1 * m_fovFactor);
		Memory::Write(m_cameraFOVScansResult[FOV3] + 1, m_newCameraFOV2);
		Memory::Write(m_cameraFOVScansResult[FOV4] + 1, m_newCameraFOV1);
	}

	inline static AngelsVsDevilsFix* s_instance_ = nullptr;
};

static std::unique_ptr<AngelsVsDevilsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AngelsVsDevilsFix>(hModule);
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