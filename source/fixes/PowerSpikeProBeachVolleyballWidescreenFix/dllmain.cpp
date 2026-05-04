#include "..\..\common\FixBase.hpp"

class PowerSpikeFix final : public FixBase
{
public:
	explicit PowerSpikeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PowerSpikeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "PowerSpikeProBeachVolleyballWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Power Spike: Pro Beach Volleyball";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "PowerSpike.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 4A ?? 2B 48 ?? 8B 95");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx + 0x1C);
				int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x20);

				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});

			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x65D5, 54);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x67C7, 54);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x6391E, 54);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionScanResult = Memory::PatternScan(ExeModule(), "DC 0D ?? ?? ?? ?? DB 05");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction Scan: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)ExeModule());

			AspectRatioAddress = Memory::GetPointerFromAddress(AspectRatioInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(AspectRatioInstructionScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioMidHook(ctx);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(ExeModule(), "D9 04 95 ?? ?? ?? ?? DC 0D");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)ExeModule());

			CameraFOVOffset = Memory::GetPointerFromAddress(CameraFOVInstructionScanResult + 3, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 7);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}
	}

private:
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	uintptr_t AspectRatioAddress = 0;
	uintptr_t CameraFOVOffset = 0;

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	double m_newAspectRatio2 = 0;

	void AspectRatioMidHook(SafetyHookContext& ctx)
	{
		double& dCurrentAspectRatio = Memory::ReadMem(AspectRatioAddress);
		m_newAspectRatio2 = (dCurrentAspectRatio / (double)m_aspectRatioScale) / (double)m_aspectRatioScale;
		FPU::FMUL(m_newAspectRatio2);
	}

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.edx * 0x4 + CameraFOVOffset);
		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;
		FPU::FLD(m_newCameraFOV);
	}

	inline static PowerSpikeFix* s_instance_ = nullptr;
};

static std::unique_ptr<PowerSpikeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<PowerSpikeFix>(hModule);
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