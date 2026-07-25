#include "..\..\common\FixBase.hpp"

class VegasMakeItBigFix final : public FixBase
{
public:
	explicit VegasMakeItBigFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~VegasMakeItBigFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "VegasMakeItBigFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Vegas: Make it Big";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "casino.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "66 A3 ?? ?? ?? ?? 8B 0D", "66 A3 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 6A ?? 52 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 51 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 6A ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 74 ?? A1 ?? ?? ?? ?? 6A ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 66 A3",
		"66 89 1D ?? ?? ?? ?? 66 89 2D");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Width Instruction 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Width1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Height Instruction 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Height1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight2] - (std::uint8_t*)ExeModule());

			m_resolutionWidth1Hook = safetyhook::create_mid(ResolutionScansResult[Width1], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = ctx.eax & 0xFFFF;
			});

			m_resolutionHeight1Hook = safetyhook::create_mid(ResolutionScansResult[Height1], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResY = ctx.eax & 0xFFFF;
				s_instance_->UpdateAR();
				s_instance_->m_resolutionWidth1Hook.disable();
				s_instance_->m_resolutionHeight1Hook.disable();
			});

			m_resolution2Hook = safetyhook::create_mid(ResolutionScansResult[WidthHeight2], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = ctx.ebx & 0xFFFF;
				s_instance_->m_newResY = ctx.ebp & 0xFFFF;
				s_instance_->UpdateAR();
				s_instance_->m_resolution2Hook.disable();
			});
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 B9 ?? ?? ?? ?? DD D8", "68 ?? ?? ?? ?? 50 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E",
		"68 ?? ?? ?? ?? 51 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "DD 05 ?? ?? ?? ?? D9 F2 68", "DD 05 ?? ?? ?? ?? A1", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 86");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			m_newCameraFOV1 = m_originalCameraFOV1 * m_fovFactor;
			m_newCameraFOV2 = m_originalCameraFOV2 * (float)m_fovFactor;

			Memory::Write(CameraFOVScansResult, FOV1, FOV2, 2, &m_newCameraFOV1);
			Memory::Write(CameraFOVScansResult[FOV3] + 1, m_newCameraFOV2);
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 8D 44 24");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(RunMultipleInstancesCheckScanResult, 6);
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr double m_originalCameraFOV1 = 0.610865233466029;
	static constexpr float m_originalCameraFOV2 = 90.0f;

	int16_t m_newResX = 0;
	int16_t m_newResY = 0;

	SafetyHookMid m_resolutionWidth1Hook{};
	SafetyHookMid m_resolutionHeight1Hook{};
	SafetyHookMid m_resolution2Hook{};

	bool m_runMultipleInstances = false;

	double m_fovFactor = 0.0;
	double m_newCameraFOV1 = 0.0;
	float m_newCameraFOV2 = 0.0f;

	std::vector<std::uint8_t*> AspectRatioScansResult;

	enum ResolutionInstructionsIndex
	{
		Width1,
		Height1,
		WidthHeight2
	};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3
	};

	void WriteStaticARs()
	{
		Memory::Write(AspectRatioScansResult, AR1, AR3, 1, m_newAspectRatio);
	}

	void UpdateAR()
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
		WriteStaticARs();
	}

	inline static VegasMakeItBigFix* s_instance_ = nullptr;
};

static std::unique_ptr<VegasMakeItBigFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<VegasMakeItBigFix>(hModule);
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