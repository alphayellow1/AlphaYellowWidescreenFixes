#include "..\..\common\FixBase.hpp"

class THCCrusadesQuestForPowerFix final : public FixBase
{
public:
	explicit THCCrusadesQuestForPowerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~THCCrusadesQuestForPowerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheHistoryChannelCrusadesQuestForPowerFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "The History Channel: Crusades - Quest for Power";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "crusades.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_biospikePcxDllModule = Memory::GetHandle("biospike_PCX.DLL");
		m_biospikePcxDllModuleName = Memory::GetModuleName(m_biospikePcxDllModule);

		auto ResolutionScanResult = Memory::PatternScan(m_biospikePcxDllModule, "A3 ?? ?? ?? ?? E9 ?? ?? ?? ?? BE ?? ?? ?? ?? 8D 44 24 ?? 8A 10 8A CA 3A 16 75 ?? 3A CB 74 ?? 8A 50 ?? 8A CA 3A 56 ?? 75 ?? 03 C7 03 F7 3A CB 75 ?? 33 C0 EB ?? 1B C0 83 D8 ?? 3B C3 75");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_biospikePcxDllModuleName.c_str(), ResolutionScanResult - reinterpret_cast<std::uint8_t*>(m_biospikePcxDllModule));

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadRegister(ctx.eax);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionScanResult + 91, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHeight = Memory::ReadRegister(ctx.eax);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteStaticAR();
			});				
		}
		else
		{
			spdlog::info("Cannot locate the resolution instructions scan memory address.");
			return;
		}

		AspectRatioScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 52 8B CB");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 97 ?? ?? ?? ?? 51");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.edi + 0x158);
				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
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

	HMODULE m_biospikePcxDllModule = nullptr;
	std::string m_biospikePcxDllModuleName = "";

	int m_currentWidth = 0;
	int m_currentHeight = 0;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_cameraFOVHook{};

	std::uint8_t* AspectRatioScanResult = nullptr;

	void WriteStaticAR()
	{
		Memory::Write(AspectRatioScanResult + 1, m_newAspectRatio);
	}

	inline static THCCrusadesQuestForPowerFix* s_instance_ = nullptr;
};

static std::unique_ptr<THCCrusadesQuestForPowerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<THCCrusadesQuestForPowerFix>(hModule);
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