#include "..\..\common\FixBase.hpp"

class RainbowSix1Fix final : public FixBase
{
public:
	explicit RainbowSix1Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RainbowSix1Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TomClancysRainbowSixWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Tom Clancy's Rainbow Six";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "RainbowSix.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "77 ?? 81 78 ?? ?? ?? ?? ?? 77", "8B 98 ?? ?? ?? ?? 8B 90");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock], 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock] + 9, 2);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.ecx + ctx.esi);
				int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x264);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}

		auto CameraFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "89 81 ?? ?? ?? ?? E8 ?? ?? ?? ?? 3C ?? 0F 94 C0 C2 ?? ?? 90 90 90 90 90 90 90 90 90 8B C1",
		"D9 83 ?? ?? ?? ?? D8 0D", "D9 82 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				const float& fCurrentCameraFOV1 = Memory::ReadRegister(ctx.eax);

				if (fCurrentCameraFOV1 == 1.8f)
				{
					s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
				}

				*reinterpret_cast<float*>(ctx.ecx + 0x181C) = s_instance_->m_newCameraFOV1;
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 6);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOV2MidHook(ctx.ebx + 0xB4);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV3], 6);

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOV2MidHook(ctx.edx + 0xB4);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3
	};

	void CameraFOV2MidHook(uintptr_t CameraFOVAddress)
	{
		float& fCurrentCameraFOV2 = Memory::ReadMem(CameraFOVAddress);

		if (fCurrentCameraFOV2 == 1.8f)
		{
			m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, m_aspectRatioScale) * m_fovFactor;
		}
		else
		{
			m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, m_aspectRatioScale);
		}

		FPU::FLD(m_newCameraFOV2);
	}

	inline static RainbowSix1Fix* s_instance_ = nullptr;
};

static std::unique_ptr<RainbowSix1Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<RainbowSix1Fix>(hModule);
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