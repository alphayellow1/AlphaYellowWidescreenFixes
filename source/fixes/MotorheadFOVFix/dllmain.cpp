#include "..\..\common\FixBase.hpp"

class MotorheadFix final : public FixBase
{
public:
	explicit MotorheadFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MotorheadFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MotorheadFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Motorhead";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "motor.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 90 20 45 03 00 89 90 A4 0A 00 00");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadMem(ctx.eax + 0x34520);
				s_instance_->m_currentHeight = Memory::ReadMem(ctx.eax + 0x34524);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteCameraFOV();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "DC 3D ?? ?? ?? ?? 31 DB");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_dNewAspectRatio = (double)s_instance_->m_newAspectRatio;
				FPU::FDIVR(s_instance_->m_dNewAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "73 08 C7 02 ?? ?? ?? ?? EB 2C", "76 19 C7 02 ?? ?? ?? ?? 89 D0 E8 ?? ?? ?? ?? B8 01 00 00 00 89 EC 5D 5A C2 04 00 8B 45 0C",
		"68 00 00 80 3F E8 ?? ?? ?? ?? B8 ?? ?? ?? ?? FF 35");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Clamp Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOVClamp1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Clamp Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOVClamp2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV] - (std::uint8_t*)ExeModule());			

			Memory::PatchBytes(CameraFOVScansResult[FOVClamp1], "\xEB");
			Memory::PatchBytes(CameraFOVScansResult[FOVClamp2], "\xEB");			
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 1.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};

	int m_currentWidth = 0;
	int m_currentHeight = 0;

	enum CameraFOVInstructionsIndices
	{
		FOVClamp1,
		FOVClamp2,
		FOV
	};

	std::vector<std::uint8_t*> CameraFOVScansResult;

	void WriteCameraFOV()
	{
		m_newCameraFOV = (m_originalCameraFOV / m_aspectRatioScale) / m_fovFactor;
		Memory::Write(CameraFOVScansResult[FOV] + 1, m_newCameraFOV);
	}

	double m_dNewAspectRatio = 0.0;

	inline static MotorheadFix* s_instance_ = nullptr;
};

static std::unique_ptr<MotorheadFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<MotorheadFix>(hModule);
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