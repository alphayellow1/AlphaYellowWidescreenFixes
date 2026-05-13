#include "..\..\common\FixBase.hpp"

class AirborneHeroFix final : public FixBase
{
public:
	explicit AirborneHeroFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AirborneHeroFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AirborneHeroDDayFrontline1944WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Airborne Hero D - Day Frontline 1944";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AB_main.exe");
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

		auto ResolutionListScanResult = Memory::PatternScan(ExeModule(), "80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(ResolutionListScanResult, m_newResX);
			Memory::Write(ResolutionListScanResult + 4, m_newResY);
			Memory::Write(ResolutionListScanResult + 8, m_newResX);
			Memory::Write(ResolutionListScanResult + 12, m_newResY);
			Memory::Write(ResolutionListScanResult + 16, m_newResX);
			Memory::Write(ResolutionListScanResult + 20, m_newResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "DC 0D ?? ?? ?? ?? DB 05");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			m_newHorizontalRes = 240 * (double)m_newAspectRatio;

			Memory::Write(AspectRatioScanResult + 2, &m_newHorizontalRes);
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction scan memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "DB 40 ?? D8 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? DB 40 ?? A1");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 3);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction scan memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	double m_newHorizontalRes = 0.0;

	float m_fCurrentCameraFOV = 0.0f;
	float m_fNewCameraFOV = 0.0f;
	uint32_t m_iNewCameraFOV = 0;

	SafetyHookMid m_cameraFOVHook{};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		uint32_t& iCurrentCameraFOV = Memory::ReadMem(ctx.eax + 0x48);

		m_fCurrentCameraFOV = (float)(iCurrentCameraFOV / 1024.0f);

		m_fNewCameraFOV = Maths::CalculateNewFOV_DegBased(m_fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;

		m_iNewCameraFOV = (uint32_t)(m_fNewCameraFOV * 1024.0f);

		FPU::FILD(m_iNewCameraFOV);
	}

	inline static AirborneHeroFix* s_instance_ = nullptr;
};

static std::unique_ptr<AirborneHeroFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AirborneHeroFix>(hModule);
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