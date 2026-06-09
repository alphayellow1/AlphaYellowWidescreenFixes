#include "..\..\common\FixBase.hpp"

class RapidGunnerFix final : public FixBase
{
public:
	explicit RapidGunnerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RapidGunnerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "RapidGunnerTacticalShooterFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Rapid Gunner: Tactical Shooter";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "engine.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 04 24 3B 05 ?? ?? ?? ?? 74 ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 8B 04 24 85 C0 8B 54 24 ?? 89 44 24 ?? DB 44 24 ?? A3");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				spdlog::info("Current res: {}x{}", iCurrentWidth, iCurrentHeight);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto HUDAspectRatioScansResult = Memory::PatternScan(ExeModule(), "C7 40 ?? ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? C3 CC 6A",
		"C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 44 24 ?? 89 4C 24 ?? 89 54 24");
		if (Memory::AreAllSignaturesValid(HUDAspectRatioScansResult) == true)
		{
			spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), HUDAspectRatioScansResult[HUDAR1] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), HUDAspectRatioScansResult[HUDAR2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(HUDAspectRatioScansResult[HUDAR1], 7);
			Memory::WriteNOPs(HUDAspectRatioScansResult[HUDAR2], 8);

			m_hudAspectRatio1Hook = safetyhook::create_mid(HUDAspectRatioScansResult[HUDAR1], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(ctx.eax + 0x2C);
			});

			m_hudAspectRatio2Hook = safetyhook::create_mid(HUDAspectRatioScansResult[HUDAR2], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(ctx.esp + 0x54);
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 01 C3 CC CC");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 2);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, CameraFOVMidHook);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalHUDWidth = 800.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_hudAspectRatio1Hook{};
	SafetyHookMid m_hudAspectRatio2Hook{};
	SafetyHookMid m_cameraFOVHook{};

	float m_newHUDWidth = 0.0f;

	enum HUDAspectRatioInstructionsIndex
	{
		HUDAR1,
		HUDAR2
	};

	static void HUDAspectRatioMidHook(uintptr_t sourceAddr)
	{
		s_instance_->m_newHUDWidth = m_originalHUDWidth * s_instance_->m_aspectRatioScale;
		*reinterpret_cast<float*>(sourceAddr) = s_instance_->m_newHUDWidth;
	}

	static void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.ecx);

		if (fCurrentCameraFOV == 90.0f)
		{
			s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
		}
		else
		{
			s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, s_instance_->m_aspectRatioScale);
		}

		FPU::FLD(s_instance_->m_newCameraFOV);
	}

	inline static RapidGunnerFix* s_instance_ = nullptr;
};

static std::unique_ptr<RapidGunnerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<RapidGunnerFix>(hModule);
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