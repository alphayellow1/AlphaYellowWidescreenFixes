#include "..\..\common\FixBase.hpp"

class SinsFix final : public FixBase
{
public:
	explicit SinsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SinsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "7SinsFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "7 Sins";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "THE7SINS_RETAIL.EXE");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 0A 89 0D ?? ?? ?? ?? 8B 4A", "8B 0A 89 0D ?? ?? ?? ?? 8B 6A");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());

			spdlog::info("Resolution Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());

			m_resolution1Hook = safetyhook::create_mid(ResolutionScansResult[Res1], [](SafetyHookContext& ctx)
			{
				s_instance_->ComputeARAndScale(ctx);
			});

			m_resolution2Hook = safetyhook::create_mid(ResolutionScansResult[Res2], [](SafetyHookContext& ctx)
			{
				s_instance_->ComputeARAndScale(ctx);
			});
		}
		
		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 40 ?? D8 0D ?? ?? ?? ?? D8 35 ?? ?? ?? ?? D8 0D",
		"D9 40 ?? D8 0D ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? 83 C4 ?? D9 5D", "D9 45 ?? D8 35 ?? ?? ?? ?? D9 5D ?? 8B 4D");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, FOV1, FOV3, 0, 3);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.eax + 0x38, AngleMode::Deg, s_instance_->m_fovFactor);
			});

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.eax + 0x44, AngleMode::Rad);
			});

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebp - 0x20, AngleMode::Rad);
			});
		}
	}

private:
	enum AngleMode
	{
		Rad,
		Deg
	};

	enum ResolutionInstructionsIndices
	{
		Res1,
		Res2
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3
	};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolution1Hook;
	SafetyHookMid m_resolution2Hook;
	SafetyHookMid m_cameraFOV1Hook;
	SafetyHookMid m_cameraFOV2Hook;
	SafetyHookMid m_cameraFOV3Hook;

	void ComputeARAndScale(SafetyHookContext& ctx)
	{
		int& iCurrentWidth = Memory::ReadMem(ctx.edx);
		int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);

		m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
	}

	void CameraFOVMidHook(uintptr_t FOVAddress, AngleMode angleMode, float fovFactor = 1.0f)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		switch (angleMode)
		{
			case AngleMode::Rad:
			{
				m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;
				break;
			}

			case AngleMode::Deg:
			{
				m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;
				break;
			}
		}

		FPU::FLD(m_newCameraFOV);
	}

	inline static SinsFix* s_instance_ = nullptr;
};

static std::unique_ptr<SinsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<SinsFix>(hModule);

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