#include "..\..\common\FixBase.hpp"

class AnimorphsFix final : public FixBase
{
public:
	explicit AnimorphsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AnimorphsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AnimorphsKnowTheSecretWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Animorphs: Know the Secret";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Animorphs.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 89 44 24",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6a ?? 6a ?? 6a ?? b9 ?? ?? ?? ?? e8 ?? ?? ?? ?? c7 05", "c7 05 ?? ?? ?? ?? ?? ?? ?? ?? c7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 0d",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6a ?? 6a ?? 6a ?? b9 ?? ?? ?? ?? e8 ?? ?? ?? ?? 8b 0d", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 50",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? b9 ?? ?? ?? ?? e8 ?? ?? ?? ?? 8b ce", "c7 42 ?? ?? ?? ?? ?? 88 42", "8B 6C 24 44 56 85 ED 75 03 8B 69 40 8B 5C 24 4C");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res5] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res6] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res7] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res8] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[Res1], 6);

			m_resolutionWidth1Hook = safetyhook::create_mid(ResolutionScansResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionScansResult[Res1] + 16, 5);

			m_resolutionHeight1Hook = safetyhook::create_mid(ResolutionScansResult[Res1] + 16, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::Write(ResolutionScansResult[Res2] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res2] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Res3] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res3] + 16, m_newResY);
			Memory::Write(ResolutionScansResult[Res4] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res4] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Res5] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res5] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Res6] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res6] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Res7] + 3, m_newResX);
			Memory::Write(ResolutionScansResult[Res7] + 13, m_newResY);

			Memory::WriteNOPs(ResolutionScansResult[Res8], 4);

			m_resolutionWidth8Hook = safetyhook::create_mid(ResolutionScansResult[Res8], [](SafetyHookContext& ctx)
			{
				ctx.ebp = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionScansResult[Res8] + 12, 4);

			m_resolutionHeight8Hook = safetyhook::create_mid(ResolutionScansResult[Res8] + 12, [](SafetyHookContext& ctx)
			{
				ctx.ebx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 74 24 ?? D9 5C 24 ?? D9 44 24 ?? D9 E0");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 4);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FDIV(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24 ??");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 4);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x4);
				s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionWidth1Hook{};
	SafetyHookMid m_resolutionHeight1Hook{};
	SafetyHookMid m_resolutionWidth8Hook{};
	SafetyHookMid m_resolutionHeight8Hook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	enum ResolutionInstructionsIndex
	{
		Res1,
		Res2,
		Res3,
		Res4,
		Res5,
		Res6,
		Res7,
		Res8
	};

	inline static AnimorphsFix* s_instance_ = nullptr;
};

static std::unique_ptr<AnimorphsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AnimorphsFix>(hModule);
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