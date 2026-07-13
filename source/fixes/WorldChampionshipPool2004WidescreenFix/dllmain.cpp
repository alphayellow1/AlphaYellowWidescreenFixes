#include "..\..\common\FixBase.hpp"

class WCP2004Fix final : public FixBase
{
public:
	explicit WCP2004Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WCP2004Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "WorldChampionshipPool2004WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1.1";
	}

	const char* TargetName() const override
	{
		return "World Championship Pool 2004";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "pool2004.exe");
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
		ResolutionScansResult = Memory::PatternScan(ExeModule(), "74 ?? 8B 04 24", "0F 85 ?? ?? ?? ?? 3D", "C7 02 ?? ?? ?? ?? 8B 04 24",
		"a1 ?? ?? ?? ?? 8b 0d ?? ?? ?? ?? 8b 15 ?? ?? ?? ?? a3 ?? ?? ?? ?? a1 ?? ?? ?? ?? a3 ?? ?? ?? ?? a1 ?? ?? ?? ?? 81 ec");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Video Setup Dialog Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[VidDialog] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Startup Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[StartupRes] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Globals Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(ResolutionScansResult[VidDialog], "\xEB");

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock], 28);

			m_resolutionWidthAddress = Memory::GetPointerFromAddress(ResolutionScansResult[WidthHeight] + 7, Memory::PointerMode::Absolute);
			m_resolutionHeightAddress = Memory::GetPointerFromAddress(ResolutionScansResult[WidthHeight] + 1, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(s_instance_->m_resolutionWidthAddress);
				s_instance_->m_newResY = Memory::ReadMem(s_instance_->m_resolutionHeightAddress);
				s_instance_->WriteStaticRes();
				s_instance_->m_resolutionHook.disable();
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 4C 24 ?? C7 44 24");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScanResult + 4, m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto MultipleInstancesScanResult = Memory::PatternScan(ExeModule(), "75 ?? A1 ?? ?? ?? ?? 50 FF 15");
			if (MultipleInstancesScanResult)
			{
				spdlog::info("Multiple Instances Check Scan: Address is {:s}+{:x}", ExeName().c_str(), MultipleInstancesScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(MultipleInstancesScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check scan memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "0F 85 ?? ?? ?? ?? 39 2D");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Check Scan: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScanResult, "\xE9\xB5\x00\x00\x00\x90");
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos check scan memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.3490999937f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	uintptr_t m_resolutionWidthAddress = 0;
	uintptr_t m_resolutionHeightAddress = 0;

	std::vector<std::uint8_t*> ResolutionScansResult;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	enum ResolutionInstructionsIndices
	{
		VidDialog,
		ListUnlock,
		StartupRes,
		WidthHeight
	};

	void WriteStaticRes()
	{
		Memory::Write(ResolutionScansResult[StartupRes] + 2, m_newResX);
		Memory::Write(ResolutionScansResult[StartupRes] + 12, m_newResY);
		Memory::Write(ResolutionScansResult[StartupRes] + 30, m_newResX);
		Memory::Write(ResolutionScansResult[StartupRes] + 25, m_newResY);
		Memory::Write(ResolutionScansResult[StartupRes] + 45, m_newResX);
		Memory::Write(ResolutionScansResult[StartupRes] + 40, m_newResY);
	}

	inline static WCP2004Fix* s_instance_ = nullptr;
};

static std::unique_ptr<WCP2004Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<WCP2004Fix>(hModule);
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