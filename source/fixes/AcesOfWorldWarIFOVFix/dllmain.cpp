#include "..\..\common\FixBase.hpp"

class AcesWW1Fix final : public FixBase
{
public:
	explicit AcesWW1Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AcesWW1Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AcesOfWorldWarIFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Aces of World War I";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "aces.exe");
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

		auto AspectRatioInstructionScanResult = Memory::PatternScan(ExeModule(), "C7 47 ?? ?? ?? ?? ?? C7 47 ?? ?? ?? ?? ?? 8B 8E");
		if (AspectRatioInstructionScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioInstructionScanResult - (std::uint8_t*)ExeModule());

			m_newAspectRatio2 = m_OriginalAspectRatio / m_aspectRatioScale;

			Memory::Write(AspectRatioInstructionScanResult + 3, m_newAspectRatio2);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D9 5F");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)ExeModule());

			m_CameraFOVAddress = Memory::GetPointerFromAddress(CameraFOVInstructionScanResult + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 6);

			m_CameraFOVHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(s_instance_->m_CameraFOVAddress);

				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;

				FPU::FMUL(s_instance_->m_newCameraFOV);
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
	static constexpr float m_OriginalAspectRatio = 0.75f;
	float m_aspectRatioScale = 1.0f;
	float m_newAspectRatio2 = 0.0f;

	uintptr_t m_CameraFOVAddress = 0;

	SafetyHookMid m_ResolutionHook{};
	SafetyHookMid m_CameraFOVHook{};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.eax + 0x114);

		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;

		FPU::FMUL(m_newCameraFOV);
	}

	inline static AcesWW1Fix* s_instance_ = nullptr;
};

static std::unique_ptr<AcesWW1Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AcesWW1Fix>(hModule);
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