#include "..\..\common\FixBase.hpp"

class GoofySkateboardingFix final : public FixBase
{
public:
	explicit GoofySkateboardingFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~GoofySkateboardingFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DisneysExtremelyGoofySkateboardingWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2.2";
	}

	const char* TargetName() const override
	{
		return "Disney's Extremely Goofy Skateboarding";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Skating.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "Framerate", m_newFramerate);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_newFramerate);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "74 ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B 44 24",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 6A ?? E8 ?? ?? ?? ?? 8B 15", "8B 35 ?? ?? ?? ?? 8B C8 A1", "68 ?? ?? ?? ?? 8B CB 75");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Hi-Res Unlock Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[HiResUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Setup Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResSetup] - (std::uint8_t*)ExeModule());
			spdlog::info("Target FPS Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[TargetFPS] - (std::uint8_t*)ExeModule());
			spdlog::info("Frame Delta Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[FrameDelta] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[HiResUnlock], 2);
			Memory::Write(ResolutionScansResult[HiResUnlock] + 8, m_newResX);
			Memory::Write(ResolutionScansResult[HiResUnlock] + 18, m_newResY);

			Memory::Write(ResolutionScansResult[ResSetup] + 5, m_newResX);
			Memory::Write(ResolutionScansResult[ResSetup] + 1, m_newResY);

			Memory::Write(ResolutionScansResult[TargetFPS] + 2, &m_newFramerate);

			m_newFrameDelta = (float)(1.0f / m_newFramerate);

			Memory::Write(ResolutionScansResult[FrameDelta] + 1, m_newFrameDelta);
			Memory::Write(ResolutionScansResult[FrameDelta] + 20, m_newFrameDelta);
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? A1 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 8B 08 FF 91 ?? ?? ?? ?? C3 90");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScanResult + 2, &m_newAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 3D ?? ?? ?? ?? D9 15 ?? ?? ?? ?? D9 C0", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C6 46 ?? ?? 8B 46");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());

			m_newGeneralFOV = 1.0f / m_aspectRatioScale;

			Memory::Write(CameraFOVScansResult[General] + 2, &m_newGeneralFOV);

			m_newGameplayFOV = m_originalGameplayFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult[Gameplay] + 1, m_newGameplayFOV);
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? 5F B8 ?? ?? ?? ?? 5E");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Run Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "0F 85 ?? ?? ?? ?? 50 A1");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Intro Videos Skip Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScanResult, "\xE9\x79\x01\x00\x00\x90");
			}
			else
			{
				spdlog::error("Failed to locate intro videos skip instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalGameplayFOV = 1.04719758f;	

	int m_newFramerate = 0;
	float m_newFrameDelta = 0.0f;
	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	float m_newGeneralFOV = 0.0f;
	float m_newGameplayFOV = 0.0f;

	enum ResolutionInstructionsIndex
	{
		HiResUnlock,
		ResSetup,
		TargetFPS,
		FrameDelta
	};

	enum CameraFOVInstructionsIndices
	{
		General,
		Gameplay
	};

	inline static GoofySkateboardingFix* s_instance_ = nullptr;
};

static std::unique_ptr<GoofySkateboardingFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<GoofySkateboardingFix>(hModule);
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