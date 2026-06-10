#include "..\..\common\FixBase.hpp"

class F1RCFix final : public FixBase
{
public:
	explicit F1RCFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~F1RCFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "F1RacingChampionshipFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "F1 Racing Championship";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "f1rc.exe");
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

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 46 ?? 51 8B 4E ?? 52 50 E8");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 3);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.esi + 0xC);

				if (fCurrentCameraFOV == 0.8944826126f || fCurrentCameraFOV == 0.8726657033f || fCurrentCameraFOV == 0.9773852825f ||
					fCurrentCameraFOV == 0.9992014766f || fCurrentCameraFOV == 1.082105041f || fCurrentCameraFOV == 0.9861115813f ||
					fCurrentCameraFOV == 1.130100012f || fCurrentCameraFOV == 0.9337521195f || fCurrentCameraFOV == 0.8988454938f) // These are all the FOVs for the different cockpit views
				{
					s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newCameraFOV = fCurrentCameraFOV;
				}

				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D9 46 ?? D8 76 ?? 8B 56", "8B 56 ?? 50 8B 46 ?? 51 8B 4E ?? 52 50");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Menu Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[MenuAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Races Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[RacesAR] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult, MenuAR, RacesAR, 0, 3);

			m_menuAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[MenuAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentMenuAspectRatio = Memory::ReadMem(ctx.esi + 0x30);
				s_instance_->m_newMenuAspectRatio = fCurrentMenuAspectRatio / s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newMenuAspectRatio);
			});

			m_racesAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[RacesAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentRacesAspectRatio = Memory::ReadMem(ctx.esi + 0x14);
				s_instance_->m_newRacesAspectRatio = fCurrentRacesAspectRatio / s_instance_->m_aspectRatioScale;
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newRacesAspectRatio);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_menuAspectRatioHook{};
	SafetyHookMid m_racesAspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	float m_newMenuAspectRatio = 0.0f;
	float m_newRacesAspectRatio = 0.0f;

	enum AspectRatioInstructionsIndex
	{
		MenuAR,
		RacesAR
	};

	inline static F1RCFix* s_instance_ = nullptr;
};

static std::unique_ptr<F1RCFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<F1RCFix>(hModule);
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