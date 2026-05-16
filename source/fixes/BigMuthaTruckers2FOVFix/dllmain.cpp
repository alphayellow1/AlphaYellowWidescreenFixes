#include "..\..\common\FixBase.hpp"

class BigMuthaTruckers2Fix final : public FixBase
{
public:
	explicit BigMuthaTruckers2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BigMuthaTruckers2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BigMuthaTruckers2FOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Big Mutha Truckers 2";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "bmt2.exe");
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
		m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? 8B 44 24 ?? D8 74 24 ?? 89 46");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult + 8 - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 4);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD((float)s_instance_->m_newResX);
			});

			Memory::WriteNOPs(AspectRatioScanResult + 8, 4);

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScanResult + 8, [](SafetyHookContext& ctx)
			{
				FPU::FDIV((float)s_instance_->m_newResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 3D ?? ?? ?? ?? D9 42", "D8 4F ?? D9 47 ?? D8 8E ?? ?? ?? ?? DE C1 D9 1C 24 E8 ?? ?? ?? ?? 8B 56 ?? 89 96 ?? ?? ?? ?? EB",
		"D8 4F ?? D9 47 ?? D8 8E ?? ?? ?? ?? DE C1 D9 1C 24 E8 ?? ?? ?? ?? 8B 56 ?? 89 96 ?? ?? ?? ?? 5F", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 ED");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Chase Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Chase] - (std::uint8_t*)ExeModule());
			spdlog::info("Cockpit Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cockpit] - (std::uint8_t*)ExeModule());
			spdlog::info("Rear View Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[RearView] - (std::uint8_t*)ExeModule());

			m_newGeneralFOV = 1.0f / m_aspectRatioScale;

			Memory::Write(CameraFOVScansResult[General] + 2, &m_newGeneralFOV);

			Memory::WriteNOPs(CameraFOVScansResult[Chase], 6);

			m_chaseFOVHook = safetyhook::create_mid(CameraFOVScansResult[Chase], [](SafetyHookContext& ctx)
			{
				s_instance_->VehicleFOVMidHook(ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cockpit], 6);

			m_cockpitFOVHook = safetyhook::create_mid(CameraFOVScansResult[Cockpit], [](SafetyHookContext& ctx)
			{
				s_instance_->VehicleFOVMidHook(ctx);
			});

			m_newRearViewFOV = m_originalRearViewFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult[RearView] + 1, m_newRearViewFOV);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalRearViewFOV = 70.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_chaseFOVHook{};
	SafetyHookMid m_cockpitFOVHook{};

	float m_newGeneralFOV = 0.0f;
	float m_newVehicleFOV1 = 0.0f;
	float m_newVehicleFOV2 = 0.0f;
	float m_newRearViewFOV = 0.0f;

	enum CameraFOVInstructionsIndex
	{
		General,
		Chase,
		Cockpit,
		RearView
	};

	void VehicleFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentVehicleFOV1 = Memory::ReadMem(ctx.edi + 0x28);
		float& fCurrentVehicleFOV2 = Memory::ReadMem(ctx.edi + 0x2C);
		m_newVehicleFOV1 = fCurrentVehicleFOV1 * m_fovFactor;
		m_newVehicleFOV2 = fCurrentVehicleFOV2 * m_fovFactor;
		FPU::FMUL(m_newVehicleFOV1);
		FPU::FLD(m_newVehicleFOV2);
	}

	inline static BigMuthaTruckers2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<BigMuthaTruckers2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BigMuthaTruckers2Fix>(hModule);
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