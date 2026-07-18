#include "..\..\common\FixBase.hpp"

class CrimeLifeGangWarsFix final : public FixBase
{
public:
	explicit CrimeLifeGangWarsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~CrimeLifeGangWarsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "CrimeLifeGangWarsFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Crime Life: Gang Wars";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "crimelife.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 86 ?? ?? ?? ?? 9C",
		"A3 ?? ?? ?? ?? E9 ?? ?? ?? ?? 68 ?? ?? ?? ?? 56 FF D7 83 C4 ?? 85 C0 75 ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? A3 ?? ?? ?? ?? E9 ?? ?? ?? ?? 68 ?? ?? ?? ?? 56 FF D7 83 C4 ?? 85 C0 75 ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 F8");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[OnResChange] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Width 2 Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ConfigRead] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Height 2 Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ConfigRead] + 37 - (std::uint8_t*)ExeModule());

			m_resolution1Hook = safetyhook::create_mid(ResolutionScansResult[OnResChange], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esi + 0x880);
				s_instance_->m_newResY = Memory::ReadMem(ctx.esi + 0x884);
				s_instance_->UpdateAR();
			});

			m_resolutionWidth2Hook = safetyhook::create_mid(ResolutionScansResult[ConfigRead], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.eax);
			});

			m_resolutionHeight2Hook = safetyhook::create_mid(ResolutionScansResult[ConfigRead] + 37, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResY = Memory::ReadRegister(ctx.eax);
				s_instance_->UpdateAR();
			});
		}

		AspectRatioScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? EB ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructions scan memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D8 2D ?? ?? ?? ?? 57 D9 05");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScanResult + 2, &m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 51");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideoScansResult = Memory::PatternScan(ExeModule(), "A0 ?? ?? ?? ?? 84 C0 74 ?? 6A ?? 6A ?? 68 ?? ?? ?? ?? 6A",
			"68 ?? ?? ?? ?? 8B F1 C6 44 24");
			if (Memory::AreAllSignaturesValid(SkipIntroVideoScansResult) == true)
			{
				spdlog::info("Startup Intro Video Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScansResult[SkipVids1] - (std::uint8_t*)ExeModule());
				spdlog::info("Startup Intro Video Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScansResult[SkipVids2] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideoScansResult[SkipVids1], "\xE9\x6B\x00\x00\x00");
				Memory::Write(SkipIntroVideoScansResult[SkipVids2] + 1, m_newFilename);
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.7853980064f;
	const char* m_newFilename = "cineintro2.bik";

	SafetyHookMid m_resolution1Hook{};
	SafetyHookMid m_resolutionWidth2Hook{};
	SafetyHookMid m_resolutionHeight2Hook{};
	SafetyHookMid m_cameraFOVHook{};

	std::uint8_t* AspectRatioScanResult;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	enum ResolutionsInstructionsIndex
	{
		OnResChange,
		ConfigRead
	};

	enum SkipIntroVideosInstructionsIndex
	{
		SkipVids1,
		SkipVids2
	};

	void UpdateAR()
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
		WriteStaticARs();
	}

	void WriteStaticARs()
	{
		Memory::Write(AspectRatioScanResult + 1, m_newAspectRatio);
		Memory::Write(AspectRatioScanResult + 8, m_newAspectRatio);
	}

	inline static CrimeLifeGangWarsFix* s_instance_ = nullptr;
};

static std::unique_ptr<CrimeLifeGangWarsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<CrimeLifeGangWarsFix>(hModule);
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