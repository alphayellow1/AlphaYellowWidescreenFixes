#include "..\..\common\FixBase.hpp"

class Barbie12DancingPrincessesFix final : public FixBase
{
public:
	explicit Barbie12DancingPrincessesFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~Barbie12DancingPrincessesFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BarbieInThe12DancingPrincessesWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Barbie in the 12 Dancing Princesses";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Barbie(TM) In The 12 Dancing Princesses.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroLogos", m_skipIntroLogos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroLogos);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "74 ?? 8B 54 24 ?? 8B 44 24 ?? 6A", "8B 02 A3 ?? ?? ?? ?? 8B 42");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock], 2);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 8B 51 ?? 50");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 16);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraHFOV = (0.5f * s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
				s_instance_->m_newCameraVFOV = 0.375f * s_instance_->m_fovFactor;

				*reinterpret_cast<float*>(ctx.esp + 0x8) = s_instance_->m_newCameraHFOV;
				*reinterpret_cast<float*>(ctx.esp + 0xC) = s_instance_->m_newCameraVFOV;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instructions scan memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "74 ?? 6A ?? E8 ?? ?? ?? ?? 8B 54 24");
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

		if (m_skipIntroLogos == true)
		{
			auto SkipIntroLogosScansResult = Memory::PatternScan(ExeModule(), "6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? B9",
			"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? B9 ?? ?? ?? ?? E9 ?? ?? ?? ?? E8 ?? ?? ?? ?? B9 ?? ?? ?? ?? E9 ?? ?? ?? ?? 6A");
			if (Memory::AreAllSignaturesValid(SkipIntroLogosScansResult) == true)
			{
				spdlog::info("Skip Intro Logos Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScansResult[0] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Logos Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScansResult[1] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(SkipIntroLogosScansResult[0], 15);
				Memory::Write(SkipIntroLogosScansResult[1] + 6, 5);
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_runMultipleInstances = false;
	bool m_skipIntroLogos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
	};

	inline static Barbie12DancingPrincessesFix* s_instance_ = nullptr;
};

static std::unique_ptr<Barbie12DancingPrincessesFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<Barbie12DancingPrincessesFix>(hModule);
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