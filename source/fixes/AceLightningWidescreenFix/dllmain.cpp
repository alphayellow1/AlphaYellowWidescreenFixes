#include "..\..\common\FixBase.hpp"

class AceLightningFix final : public FixBase
{
public:
	explicit AceLightningFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AceLightningFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AceLightningWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Ace Lightning";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "BBC - Ace Lightning.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 0D",
		"8B 4E ?? 89 54 24 ?? 8B 56", "8B 4D ?? A1 ?? ?? ?? ?? 89 4C 24 ?? 8B 55 ??", "8B 70 ?? 8B 78");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Renderer] - (std::uint8_t*)ExeModule());
			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Viewport1] - (std::uint8_t*)ExeModule());
			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Viewport2] - (std::uint8_t*)ExeModule());
			spdlog::info("Viewport Resolution Instructions 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Viewport3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[Renderer], 12);

			m_RendererResHook = safetyhook::create_mid(ResolutionScansResult[Renderer], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);

				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::WriteNOPs(ResolutionScansResult[Viewport1], 3);

			m_ViewportWidth1Hook = safetyhook::create_mid(ResolutionScansResult[Viewport1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionScansResult[Viewport1] + 7, 3);

			m_ViewportHeight1Hook = safetyhook::create_mid(ResolutionScansResult[Viewport1] + 7, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::WriteNOPs(ResolutionScansResult[Viewport2], 3);

			m_ViewportWidth2Hook = safetyhook::create_mid(ResolutionScansResult[Viewport2], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionScansResult[Viewport2] + 12, 3);

			m_ViewportHeight2Hook = safetyhook::create_mid(ResolutionScansResult[Viewport2] + 12, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::WriteNOPs(ResolutionScansResult[Viewport3], 6);

			m_Viewport3Hook = safetyhook::create_mid(ResolutionScansResult[Viewport3], [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
				ctx.edi = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}

		auto AspectRatioAndHUDScanResult = Memory::PatternScan(ExeModule(), "8B 08 D9 05 ?? ?? ?? ?? 89");
		if (AspectRatioAndHUDScanResult)
		{
			spdlog::info("Aspect Ratio & HUD Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioAndHUDScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioAndHUDScanResult, 2);

			m_AspectRatioAndHUDHook = safetyhook::create_mid(AspectRatioAndHUDScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = Memory::ReadMem(ctx.eax);

				if (fCurrentCameraHFOV == 0.5f || fCurrentCameraHFOV == 0.129999995232f || fCurrentCameraHFOV == 0.800000011921f)
				{
					s_instance_->m_newCameraHFOV = fCurrentCameraHFOV;
				}
				else
				{
					s_instance_->m_newCameraHFOV = fCurrentCameraHFOV * s_instance_->m_aspectRatioScale;
				}

				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraHFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio & HUD instruction memory address.");
			return;
		}

		auto CameraFOVInstructionsScanResult = Memory::PatternScan(ExeModule(), "D8 88 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 80 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE C1 D9 98 ?? ?? ?? ?? D9",
		"D8 88 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 80 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE C1 D9 98 ?? ?? ?? ?? C3");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScanResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScanResult[HFOV] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScanResult[VFOV] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionsScanResult, HFOV, VFOV, 0, 6);

			m_CameraHFOVHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[HFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
			});

			m_CameraVFOVHook = safetyhook::create_mid(CameraFOVInstructionsScanResult[VFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;

	SafetyHookMid m_RendererResHook{};
	SafetyHookMid m_ViewportWidth1Hook{};
	SafetyHookMid m_ViewportHeight1Hook{};
	SafetyHookMid m_ViewportWidth2Hook{};
	SafetyHookMid m_ViewportHeight2Hook{};
	SafetyHookMid m_Viewport3Hook{};
	SafetyHookMid m_AspectRatioAndHUDHook{};
	SafetyHookMid m_CameraHFOVHook{};
	SafetyHookMid m_CameraVFOVHook{};

	enum ResolutionInstructionsIndices
	{
		Renderer,
		Viewport1,
		Viewport2,
		Viewport3
	};

	enum CameraFOVInstructionsIndices
	{
		HFOV,
		VFOV
	};

	float m_newCameraHFOV = 0.0f;		

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.eax + 0x114);

		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;

		FPU::FMUL(m_newCameraFOV);
	}

	inline static AceLightningFix* s_instance_ = nullptr;
};

static std::unique_ptr<AceLightningFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AceLightningFix>(hModule);
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