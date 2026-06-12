#include "..\..\common\FixBase.hpp"

class CTSpecialForcesFix final : public FixBase
{
public:
	explicit CTSpecialForcesFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~CTSpecialForcesFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "CTSpecialForcesFireForEffectWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "CT Special Forces: Fire for Effect";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Overlay.exe") ||
		Util::stringcmp_caseless(exeName, "CT Special Forces.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "CT Special Forces.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "0F 8A ?? ?? ?? ?? D8 1D");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 6);
				Memory::WriteNOPs(ResolutionListUnlockScanResult + 17, 6);
			}
			else
			{
				spdlog::error("Failed to find resolution list unlock instruction memory address.");
				return;
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Overlay.exe"))
		{
			auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 4C 24 ?? 8B 54 24 ?? 50 51 52 8B CE");
			if (CameraFOVScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVScanResult, 4);

				m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x2C);

					if (fCurrentCameraFOV == 1.374446869f)
					{
						s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV = fCurrentCameraFOV;
					}

					ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
				});
			}
			else
			{
				spdlog::error("Failed to find camera FOV instruction memory address.");
				return;
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_cameraFOVHook{};

	inline static CTSpecialForcesFix* s_instance_ = nullptr;
};

static std::unique_ptr<CTSpecialForcesFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<CTSpecialForcesFix>(hModule);
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