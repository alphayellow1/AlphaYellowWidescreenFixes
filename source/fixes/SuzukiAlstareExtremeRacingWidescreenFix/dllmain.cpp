#include "..\..\common\FixBase.hpp"

class SAERFix final : public FixBase
{
public:
	explicit SAERFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SAERFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SuzukiAlstareExtremeRacingWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Suzuki Alstare Extreme Racing";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "SaerPC.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionListScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 81 7E");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListScanResult - (std::uint8_t*)ExeModule());

			// 640x480
			Memory::Write(ResolutionListScanResult + 6, m_newResX);
			Memory::Write(ResolutionListScanResult + 1, m_newResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 74, m_newResX);
			Memory::Write(ResolutionListScanResult + 69, m_newResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 95, m_newResX);
			Memory::Write(ResolutionListScanResult + 90, m_newResY);

			// 1152x864
			Memory::Write(ResolutionListScanResult + 115, m_newResX);
			Memory::Write(ResolutionListScanResult + 110, m_newResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 136, m_newResX);
			Memory::Write(ResolutionListScanResult + 131, m_newResY);

			// 1600x1200
			Memory::Write(ResolutionListScanResult + 160, m_newResX);
			Memory::Write(ResolutionListScanResult + 155, m_newResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list scan memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D9 5C 24 ?? 75", "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0E");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Races Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[RacesAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[MainMenuAR] - (std::uint8_t*)ExeModule());

			m_newGameplayAspectRatio = 0.7518796921f / m_aspectRatioScale;

			Memory::Write(AspectRatioScansResult[RacesAR] + 2, &m_newGameplayAspectRatio);
			Memory::Write(AspectRatioScansResult[MainMenuAR] + 4, m_newAspectRatio);
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "DB 05 ?? ?? ?? ?? A1 ?? ?? ?? ?? BD");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = static_cast<int>(std::lround((150.0f * (((1.0f + 40.0f / 150.0f) * 0.75f * m_newAspectRatio) - 1.0f)) * m_fovFactor));

			Memory::Write(CameraFOVScanResult + 2, &m_newCameraFOV);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "8B 4C DC ?? 51");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Scan: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScanResult, "\xEB\x78\x90\x90");
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos scan memory address.");
				return;
			}
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesScansResult = Memory::PatternScan(ExeModule(), "75 ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 56",
			"75 ?? 81 C6 ?? ?? ?? ?? 6A");
			if (Memory::AreAllSignaturesValid(RunMultipleInstancesScansResult) == true)
			{
				spdlog::info("Run Multiple Instances Check 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesScansResult[RunMultipleInstances1] - (std::uint8_t*)ExeModule());
				spdlog::info("Run Multiple Instances Check 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesScansResult[RunMultipleInstances2] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesScansResult[RunMultipleInstances1], "\xEB");
				Memory::PatchBytes(RunMultipleInstancesScansResult[RunMultipleInstances2], "\xEB");
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalMenuFOV = 0.5f;

	bool m_skipIntroVideos = false;
	bool m_runMultipleInstances = false;

	uintptr_t m_cameraFOVAddress = 0;

	int m_newCameraFOV = 0;

	float m_newGameplayAspectRatio = 0.0f;

	SafetyHookMid m_racesAspectRatioHook{};
	SafetyHookMid m_mainMenuAspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	enum AspectRatioInstructionsIndices
	{
		RacesAR,
		MainMenuAR
	};

	enum RunMultipleInstancesIndices
	{
		RunMultipleInstances1,
		RunMultipleInstances2
	};

	inline static SAERFix* s_instance_ = nullptr;
};

static std::unique_ptr<SAERFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<SAERFix>(hModule);
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