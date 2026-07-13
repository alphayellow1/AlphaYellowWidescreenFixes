#include "..\..\common\FixBase.hpp"

class USSpecialForcesTeamFactorFix final : public FixBase
{
public:
	explicit USSpecialForcesTeamFactorFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~USSpecialForcesTeamFactorFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "USSpecialForcesTeamFactorWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "US Special Forces: Team Factor";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "tf.exe") ||
		Util::stringcmp_caseless(exeName, "TFLoader.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntro", m_skipIntroVideo);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_zoomFactor);
		spdlog_confparse(m_skipIntroVideo);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "TFLoader.exe"))
		{
			m_tfLoaderDllModule = Memory::GetHandle({ "TFLoader-en.dll", "TFLoader-fr.dll", "TFLoader-ge.dll" });
			m_tfLoaderDllModuleName = Memory::GetModuleName(m_tfLoaderDllModule);

			auto ResolutionStringScanResult = Memory::PatternScan(m_tfLoaderDllModule, "31 32 38 30 20 78 20 31 30 32 34");
			if (ResolutionStringScanResult)
			{
				spdlog::info("Resolution String Scan: Address is {:s}+{:x}", m_tfLoaderDllModuleName.c_str(), ResolutionStringScanResult - (std::uint8_t*)m_tfLoaderDllModule);

				if (Maths::digitCount(m_newResX) == 4 && Maths::digitCount(m_newResY) == 4)
				{
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult, m_newResX);
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, m_newResY);
				}

				if (Maths::digitCount(m_newResX) == 4 && Maths::digitCount(m_newResY) == 3)
				{
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult, m_newResX);
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, m_newResY);
					Memory::PatchBytes(ResolutionStringScanResult + 10, "\x00");
				}

				if (Maths::digitCount(m_newResX) == 3 && Maths::digitCount(m_newResY) == 3)
				{
					Memory::PatchBytes(ResolutionStringScanResult, "\x00");
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 1, m_newResX);
					Memory::WriteNumberAsChar8Digits(ResolutionStringScanResult + 7, m_newResY);
					Memory::PatchBytes(ResolutionStringScanResult + 10, "\x00");
				}
			}
			else
			{
				spdlog::error("Failed to locate resolution string memory address.");
				return;
			}

			if (m_skipIntroVideo == true)
			{
				auto SkipIntroVideoScanResult = Memory::PatternScan(ExeModule(), "FF D3 85 C0 74 ?? 8B 54 24");
				if (SkipIntroVideoScanResult)
				{
					spdlog::info("Skip Intro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionStringScanResult - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(SkipIntroVideoScanResult, "\x83\xC4\x28\xEB\x0E\x90");
				}
				else
				{
					spdlog::error("Failed to locate skip intro video instruction memory address.");
					return;
				}
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "tf.exe"))
		{
			m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
			m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

			auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "BD ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? BD ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? BD ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? BD ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? BD ?? ?? ?? ?? BE ?? ?? ?? ?? EB",
			"00 05 00 00 00 04 00 00 DB 0F 49 40 DB 0F 49 40 DB 0F", "00 05 00 00 00 04 00 00 90 A2 4A 00 B0 A2 4A 00");
			if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
			{
				spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResList] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res3] - (std::uint8_t*)ExeModule());

				Memory::Write(ResolutionScansResult[ResList] + 49, m_newResX);
				Memory::Write(ResolutionScansResult[ResList] + 54, m_newResY);
				Memory::Write(ResolutionScansResult[Res2], m_newResX);
				Memory::Write(ResolutionScansResult[Res2] + 4, m_newResY);
				Memory::Write(ResolutionScansResult[Res3], m_newResX);
				Memory::Write(ResolutionScansResult[Res3] + 4, m_newResY);
			}

			auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "DC 3D ?? ?? ?? ?? D9 5C 24");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());				

				m_newAspectRatio2 = 1.0 / (double)m_aspectRatioScale;

				Memory::Write(AspectRatioScanResult + 2, &m_newAspectRatio2);
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction memory address.");
				return;
			}

			auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D8 05 ?? ?? ?? ?? D9 5C 24 ?? D9 44 24",
			"D8 25 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? A1 ?? ?? ?? ?? 85 C0 75 ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 89 44 24 ?? 85 C0 C7 44 24 ?? ?? ?? ?? ?? 74 ?? 8B C8 E8 ?? ?? ?? ?? EB ?? 33 C0 C7 44 24 ?? ?? ?? ?? ?? A3 ?? ?? ?? ?? 8B 8E",
			"D8 25 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? E9",
			"D8 25 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? A1 ?? ?? ?? ?? 85 C0 75 ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 89 44 24 ?? 85 C0 C7 44 24 ?? ?? ?? ?? ?? 74 ?? 8B C8 E8 ?? ?? ?? ?? EB ?? 33 C0 C7 44 24 ?? ?? ?? ?? ?? A3 ?? ?? ?? ?? 8B 96",
			"D8 25 ?? ?? ?? ?? D8 9E");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Hipfire FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire] - (std::uint8_t*)ExeModule());
				spdlog::info("Zoom FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom1] - (std::uint8_t*)ExeModule());
				spdlog::info("Zoom FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom2] - (std::uint8_t*)ExeModule());
				spdlog::info("Zoom FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom3] - (std::uint8_t*)ExeModule());
				spdlog::info("Zoom FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom4] - (std::uint8_t*)ExeModule());

				m_newHipfireFOV = m_originalHipfireFOV * m_fovFactor;
				m_newTargetZoom = m_originalTargetZoom / m_zoomFactor;
				Memory::Write(CameraFOVScansResult[Hipfire] + 2, &m_newHipfireFOV);
				Memory::Write(CameraFOVScansResult, Zoom1, Zoom4, 2, &m_newTargetZoom);
			}
		}
	}

private:
	HMODULE m_tfLoaderDllModule = nullptr;
	std::string m_tfLoaderDllModuleName = "";

	SafetyHookMid m_skipIntroVideoHook{};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalHipfireFOV = 0.96f;
	static constexpr float m_originalTargetZoom = 100.0f;

	bool m_skipIntroVideo = false;

	double m_newAspectRatio2 = 0.0f;
	float m_zoomFactor = 0.0f;
	float m_newHipfireFOV = 0.0f;
	float m_newTargetZoom = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ResList,
		Res2,
		Res3
	};

	enum CameraFOVInstructionsIndex
	{
		Hipfire,
		Zoom1,
		Zoom2,
		Zoom3,
		Zoom4
	};

	inline static USSpecialForcesTeamFactorFix* s_instance_ = nullptr;
};

static std::unique_ptr<USSpecialForcesTeamFactorFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<USSpecialForcesTeamFactorFix>(hModule);
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