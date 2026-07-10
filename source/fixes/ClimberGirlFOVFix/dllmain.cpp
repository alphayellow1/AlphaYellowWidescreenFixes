#include "..\..\common\FixBase.hpp"

class ClimberGirlFix final : public FixBase
{
public:
	explicit ClimberGirlFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ClimberGirlFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ClimberGirlFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Climber Girl";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ClimberGirl.exe") ||
		Util::stringcmp_caseless(exeName, "Source.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 8B 4C 24 ?? 8B 7C 24 ?? 57");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x24);
				s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x28);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				spdlog::info("Current res: {}x{}", s_instance_->m_newResX, s_instance_->m_newResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 45 D8 F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 45 DC F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 45 E4");
		if (CameraFOVScansResult)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult - (std::uint8_t*)ExeModule());
			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult - (std::uint8_t*)ExeModule());
			spdlog::info("Camera VFOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult - (std::uint8_t*)ExeModule());
			spdlog::info("Camera VFOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, 8);

			m_cameraHFOV1Hook = safetyhook::create_mid(CameraFOVScansResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newHFOV1 = m_originalHFOV1 * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				ctx.xmm0.f32[0] = s_instance_->m_newHFOV1;
			});

			Memory::WriteNOPs(CameraFOVScansResult + 13, 8);

			m_cameraHFOV2Hook = safetyhook::create_mid(CameraFOVScansResult + 13, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newHFOV2 = m_originalHFOV2 * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				ctx.xmm0.f32[0] = s_instance_->m_newHFOV2;
			});

			Memory::WriteNOPs(CameraFOVScansResult + 26, 8);

			m_cameraVFOV1Hook = safetyhook::create_mid(CameraFOVScansResult + 26, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newVFOV1 = m_originalVFOV1 * s_instance_->m_fovFactor;
				ctx.xmm0.f32[0] = s_instance_->m_newVFOV1;
			});

			Memory::WriteNOPs(CameraFOVScansResult + 39, 8);

			m_cameraVFOV2Hook = safetyhook::create_mid(CameraFOVScansResult + 39, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newVFOV2 = m_originalVFOV2 * s_instance_->m_fovFactor;
				ctx.xmm0.f32[0] = s_instance_->m_newVFOV2;
			});
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "75 ?? 8D 85 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? EB");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScanResult, "\xEB\x79");
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalHFOV1 = -0.8f;
	static constexpr float m_originalHFOV2 = 0.8f;
	static constexpr float m_originalVFOV1 = -0.6f;
	static constexpr float m_originalVFOV2 = 0.6f;

	bool m_skipIntroVideos = false;

	float m_newHFOV1 = 0.0f;
	float m_newHFOV2 = 0.0f;
	float m_newVFOV1 = 0.0f;
	float m_newVFOV2 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraHFOV1Hook{};
	SafetyHookMid m_cameraHFOV2Hook{};
	SafetyHookMid m_cameraVFOV1Hook{};
	SafetyHookMid m_cameraVFOV2Hook{};

	inline static ClimberGirlFix* s_instance_ = nullptr;
};

static std::unique_ptr<ClimberGirlFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<ClimberGirlFix>(hModule);
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