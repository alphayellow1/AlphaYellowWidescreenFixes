#include "..\..\common\FixBase.hpp"

class SeaWorld2Fix final : public FixBase
{
public:
	explicit SeaWorld2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SeaWorld2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SeaWorldAdventureParksTycoon2FOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "SeaWorld Adventure Parks Tycoon 2";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "swt3d.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 4D ?? 8B 11 53");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0x5FA120);
				uint32_t& iCurrentHeight = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0x5FA124);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->WriteStaticAR();
				s_instance_->m_resolutionHook.disable();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		AspectRatioScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 B9");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());			
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructions scan memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "DD 05 ?? ?? ?? ?? D9 F2 5F");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * (double)m_fovFactor;

			Memory::Write(CameraFOVScanResult + 2, &m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr double m_originalCameraFOV = 0.610865233466029;

	SafetyHookMid m_resolutionHook{};

	uint8_t* AspectRatioScanResult = nullptr;

	uintptr_t m_cameraFOVAddress = 0;

	double m_newCameraFOV = 0.0;

	void WriteStaticAR()
	{
		Memory::Write(AspectRatioScanResult + 1, m_newAspectRatio);

		Memory::Write(AspectRatioScanResult + 31, m_newAspectRatio);
	}

	inline static SeaWorld2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<SeaWorld2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<SeaWorld2Fix>(hModule);
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