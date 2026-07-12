#include "..\..\common\FixBase.hpp"

class FastLanesBowlingFix final : public FixBase
{
public:
	explicit FastLanesBowlingFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~FastLanesBowlingFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "FastLanesBowlingWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Fast Lanes Bowling";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "FastLanesDX9Test.exe") ||
		Util::stringcmp_caseless(exeName, "pcbowl.exe") ||
		Util::stringcmp_caseless(exeName, "reschange.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroLogos", m_skipIntroLogos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroLogos);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "reschange.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "81 F9 ?? ?? ?? ?? 7C ?? 83 7C 24");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 40);
			}
			else
			{
				spdlog::error("Failed to find resolution list unlock scan memory address.");
				return;
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "FastLanesDX9Test.exe") || Util::stringcmp_caseless(ExeName(), "pcbowl.exe"))
		{
			auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
				{
					const int& iCurrentWidth = Memory::ReadRegister(ctx.ecx);
					const int& iCurrentHeight = Memory::ReadRegister(ctx.edx);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->WriteStaticARs();
					s_instance_->WriteStaticFOVs();
					s_instance_->m_resolutionHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to find resolution instructions scan memory address.");
				return;
			}

			AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24",
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50",
			"C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 11");
			if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
			{
				spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
				spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
				spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
				spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());				
			}			

			if (Util::stringcmp_caseless(ExeName(), "FastLanesDX9Test.exe"))
			{
				CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24", "68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6",
				"68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50");
			}
			else if (Util::stringcmp_caseless(ExeName(), "pcbowl.exe"))
			{
				CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 8D 4C 24 ?? C7 44 24", "68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 33 F6",
				"68 ?? ?? ?? ?? 8D 54 24 ?? 89 44 24 ?? 8B 47 ?? 52 50", "68 ?? ?? ?? ?? 51 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B 15", "68 ?? ?? ?? ?? 51 50 E8 ?? ?? ?? ?? 83 C4 ?? 39 74 24");
			}

			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
				spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

				if (Util::stringcmp_caseless(ExeName(), "pcbowl.exe"))
				{
					spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
					spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
				}				
			}

			if (m_skipIntroLogos == true)
			{
				if (Util::stringcmp_caseless(ExeName(), "FastLanesDX9Test.exe"))
				{
					SkipIntroLogosScanResult = Memory::PatternScan(ExeModule(), "74 ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B 15", "74 0A C7 05 80 49 58 00 01 00 00 00 8B 0D 04 FE 54 00 6A 01",
					"74 0A C7 05 50 97 58 00 01 00 00 00 8B 0D 04 FE 54 00 6A 01", "6A ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 55");
				}
				else if (Util::stringcmp_caseless(ExeName(), "pcbowl.exe"))
				{
					SkipIntroLogosScanResult = Memory::PatternScan(ExeModule(), "74 ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B 15", "74 0A C7 05 30 15 59 00 01 00 00 00 8B 0D F4 7B 55 00",
					"74 0A C7 05 60 C7 58 00 01 00 00 00 8B 0D F4 7B 55 00 6A 01", "6A ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 55");
				}
				
				if (Memory::AreAllSignaturesValid(SkipIntroLogosScanResult) == true)
				{
					spdlog::info("Skip Intro Videos Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScanResult[SkipLogos1] - (std::uint8_t*)ExeModule());
					spdlog::info("Skip Intro Videos Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScanResult[SkipLogos2] - (std::uint8_t*)ExeModule());
					spdlog::info("Skip Intro Videos Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScanResult[SkipLogos3] - (std::uint8_t*)ExeModule());
					spdlog::info("Skip Intro Videos Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScanResult[SkipLogos4] - (std::uint8_t*)ExeModule());

					Memory::WriteNOPs(SkipIntroLogosScanResult[SkipLogos1], 2);
					Memory::WriteNOPs(SkipIntroLogosScanResult[SkipLogos2], 2);
					Memory::WriteNOPs(SkipIntroLogosScanResult[SkipLogos3], 2);
					Memory::Write(SkipIntroLogosScanResult[SkipLogos4] + 1, m_fadeDuration);
				}
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.4f;
	static constexpr int8_t m_fadeDuration = 1;

	bool m_skipIntroLogos = false;

	float m_newAspectRatio4 = 0.0f;

	std::vector<std::uint8_t*> AspectRatioScansResult;
	std::vector<std::uint8_t*> CameraFOVScansResult;
	std::vector<std::uint8_t*> SkipIntroLogosScanResult;

	SafetyHookMid m_resolutionHook{};

	enum AspectRatioInstructionsScansIndices
	{
		AR1,
		AR2,
		AR3,
		AR4
	};

	enum CameraFOVInstructionsScansIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5
	};

	enum SkipLogoInstructionsIndices
	{
		SkipLogos1,
		SkipLogos2,
		SkipLogos3,
		SkipLogos4
	};

	void WriteStaticARs()
	{
		m_newAspectRatio4 = 0.4f * m_aspectRatioScale;
		Memory::Write(AspectRatioScansResult, AR1, AR3, 1, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR4] + 4, m_newAspectRatio4);
	}

	void WriteStaticFOVs()
	{
		m_newCameraFOV = m_originalCameraFOV * m_aspectRatioScale;

		Memory::Write(CameraFOVScansResult[FOV1] + 1, m_newCameraFOV);
		Memory::Write(CameraFOVScansResult[FOV2] + 1, m_newCameraFOV * m_fovFactor);
		Memory::Write(CameraFOVScansResult[FOV3] + 1, m_newCameraFOV);

		if (Util::stringcmp_caseless(ExeName(), "pcbowl.exe"))
		{
			Memory::Write(CameraFOVScansResult[FOV4] + 1, m_newCameraFOV);
			Memory::Write(CameraFOVScansResult[FOV5] + 1, m_newCameraFOV);
		}
	}

	inline static FastLanesBowlingFix* s_instance_ = nullptr;
};

static std::unique_ptr<FastLanesBowlingFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<FastLanesBowlingFix>(hModule);
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