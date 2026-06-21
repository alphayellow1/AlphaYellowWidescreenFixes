#include "..\..\common\FixBase.hpp"

class EuroManFix final : public FixBase
{
public:
	explicit EuroManFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~EuroManFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EuroManFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.0";
	}

	const char* TargetName() const override
	{
		return "Euro-Man";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "euroman.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 4C 24 ?? 8B 54 24 ?? 8B 44 24 ?? 89 11");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x18);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x1C);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 8D ?? ?? ?? ?? 89 8A ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? 89 82 ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? 89 8A");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult + 12 - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 6);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCameraHFOV = Memory::ReadMem(ctx.ebp + 0x134);

				if (s_instance_->m_currentCameraHFOV == 62.0f)
				{
					s_instance_->m_newCameraHFOV = Maths::CalculateNewHFOV_DegBased(s_instance_->m_currentCameraHFOV, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
				}
				else
				{
					s_instance_->m_newCameraHFOV = Maths::CalculateNewHFOV_DegBased(s_instance_->m_currentCameraHFOV, s_instance_->m_aspectRatioScale);
				}

				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVScanResult + 12, 6);

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScanResult + 12, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCameraVFOV = Memory::ReadMem(ctx.ebp + 0x138);

				if (s_instance_->m_currentCameraVFOV == 48.516998291016f)
				{
					s_instance_->m_newCameraVFOV = Maths::CalculateNewVFOV_DegBased(s_instance_->m_currentCameraVFOV, s_instance_->m_fovFactor);
				}
				else
				{
					s_instance_->m_newCameraVFOV = s_instance_->m_currentCameraVFOV;
				}

				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraVFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	enum CameraFOVInstructionsIndices
	{
		HFOV,
		VFOV
	};

	float m_currentCameraHFOV = 0.0f;
	float m_currentCameraVFOV = 0.0f;
	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	inline static EuroManFix* s_instance_ = nullptr;
};

static std::unique_ptr<EuroManFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<EuroManFix>(hModule);
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