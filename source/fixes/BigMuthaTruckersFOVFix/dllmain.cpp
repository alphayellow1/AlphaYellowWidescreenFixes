#include "..\..\common\FixBase.hpp"

class BigMuthaTruckersFix final : public FixBase
{
public:
	explicit BigMuthaTruckersFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BigMuthaTruckersFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BigMuthaTruckersFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Big Mutha Truckers";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "bmt.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B F8 C7 44 24");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_currentWidthAddress = Memory::GetPointerFromAddress(ResolutionScanResult + 2, Memory::PointerMode::Absolute);
			m_currentHeightAddress = Memory::GetPointerFromAddress(ResolutionScanResult + 8, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->ResolutionMidHook(ctx);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		std::uint8_t* AspectRatioInstructionsScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 74 24 ?? 8B 4C 24");
		if (AspectRatioInstructionsScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioInstructionsScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioInstructionsScanResult, 8);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD((float)s_instance_->m_currentWidth);
				FPU::FDIV((float)s_instance_->m_currentHeight);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 3D ?? ?? ?? ?? D9 42 ?? D9 42", "D8 4B ?? D9 43 ?? D8 8E", "D8 4F ?? D9 47 ?? D8 8E",
		"68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 8E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Chase Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Chase] - (std::uint8_t*)ExeModule());
			spdlog::info("Cockpit Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cockpit] - (std::uint8_t*)ExeModule());
			spdlog::info("Rear View Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[RearView] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 6);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGeneralFOV = 1.0f / s_instance_->m_aspectRatioScale;

				FPU::FDIVR(s_instance_->m_newGeneralFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Chase], 6);

			m_chaseFOVHook = safetyhook::create_mid(CameraFOVScansResult[Chase], [](SafetyHookContext& ctx)
			{
				s_instance_->VehicleFOVMidHook(ctx.ebx + 0x28, ctx.ebx + 0x2C);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cockpit], 6);

			m_cockpitFOVHook = safetyhook::create_mid(CameraFOVScansResult[Cockpit], [](SafetyHookContext& ctx)
			{
				s_instance_->VehicleFOVMidHook(ctx.edi + 0x28, ctx.edi + 0x2C);
			});

			m_newRearViewFOV = m_originalRearViewFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult[RearView] + 1, m_newRearViewFOV);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalRearViewFOV = 70.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_chaseFOVHook{};
	SafetyHookMid m_cockpitFOVHook{};

	uintptr_t m_currentWidthAddress = 0;
	uintptr_t m_currentHeightAddress = 0;

	uint32_t m_currentWidth = 0;
	uint32_t m_currentHeight = 0;
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

	void ResolutionMidHook(SafetyHookContext& ctx)
	{
		m_currentWidth = Memory::ReadMem(m_currentWidthAddress);
		m_currentHeight = Memory::ReadMem(m_currentHeightAddress);
		m_newAspectRatio = static_cast<float>(m_currentWidth) / static_cast<float>(m_currentHeight);
		m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
	}

	void VehicleFOVMidHook(uintptr_t FOVAddress1, uintptr_t FOVAddress2)
	{
		float& fCurrentVehicleFOV1 = Memory::ReadMem(FOVAddress1);
		float& fCurrentVehicleFOV2 = Memory::ReadMem(FOVAddress2);
		m_newVehicleFOV1 = fCurrentVehicleFOV1 * m_fovFactor;
		m_newVehicleFOV2 = fCurrentVehicleFOV2 * m_fovFactor;
		FPU::FMUL(m_newVehicleFOV1);
		FPU::FLD(m_newVehicleFOV2);
	}

	inline static BigMuthaTruckersFix* s_instance_ = nullptr;
};

static std::unique_ptr<BigMuthaTruckersFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BigMuthaTruckersFix>(hModule);
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