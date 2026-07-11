#include "..\..\common\FixBase.hpp"

class NickelodeonPartyBlastFix final : public FixBase
{
public:
	explicit NickelodeonPartyBlastFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~NickelodeonPartyBlastFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "NickelodeonPartyBlastWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Nickelodeon: Party Blast";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "nGames_PC.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? B8 ?? ?? ?? ?? 5F",
		"BF ?? ?? ?? ?? 56 68");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());

			Memory::Write(ResolutionScansResult[Res1] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res1] + 16, m_newResY);
			Memory::Write(ResolutionScansResult[Res2] + 34, m_newResX);
			Memory::Write(ResolutionScansResult[Res2] + 1, m_newResY);
		}
		
		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? 85 C0 74 ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 8B 44 24");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScanResult + 56, (float)m_newResX);
			Memory::Write(AspectRatioScanResult + 4, (float)m_newResY);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 54 24 ?? 8B 07 D9 9F");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 4);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCameraFOV = Memory::ReadMem(ctx.esp + 0x10);
				s_instance_->m_newCameraFOV = s_instance_->m_currentCameraFOV * s_instance_->m_fovFactor;
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesScanResult = Memory::PatternScan(ExeModule(), "75 ?? 33 C0 5E 81 C4 ?? ?? ?? ?? C2 ?? ?? E8");
			if (RunMultipleInstancesScanResult)
			{
				spdlog::info("Multiple Instances Check Scan Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check scan instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Scan Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(SkipIntroVideosScanResult, 5);
				Memory::WriteNOPs(SkipIntroVideosScanResult + 12, 5);
				Memory::WriteNOPs(SkipIntroVideosScanResult + 24, 5);
				Memory::WriteNOPs(SkipIntroVideosScanResult + 36, 5);
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos scan instruction memory address.");
				return;
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	float m_currentCameraFOV = 0.0f;

	enum ResolutionInstructionsIndex
	{
		Res1,
		Res2
	};

	inline static NickelodeonPartyBlastFix* s_instance_ = nullptr;
};

static std::unique_ptr<NickelodeonPartyBlastFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<NickelodeonPartyBlastFix>(hModule);
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