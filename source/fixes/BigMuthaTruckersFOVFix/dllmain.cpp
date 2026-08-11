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
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Big Mutha Truckers";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "bmt.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_resolutionScanResult = Memory::PatternScan(ExeModule(), "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B F8 C7 44 24");
		if (m_resolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScanResult - (std::uint8_t*)ExeModule());

			m_currentWidthAddress = Memory::GetPointerFromAddress(m_resolutionScanResult + 2, Memory::PointerMode::Absolute);
			m_currentHeightAddress = Memory::GetPointerFromAddress(m_resolutionScanResult + 8, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(m_resolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->ResolutionMidHook(ctx);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		m_aspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 74 24 ?? 8B 4C 24");
		if (m_aspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_aspectRatioScanResult, 8);

			m_aspectRatioHook = safetyhook::create_mid(m_aspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD((float)s_instance_->m_newResX);
				FPU::FDIV((float)s_instance_->m_newResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 3D ?? ?? ?? ?? D9 42 ?? D9 42", "D8 4B ?? D9 43 ?? D8 8E", "D8 4F ?? D9 47 ?? D8 8E",
		"68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 8E");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Chase Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[Chase] - (std::uint8_t*)ExeModule());
			spdlog::info("Cockpit Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[Cockpit] - (std::uint8_t*)ExeModule());
			spdlog::info("Rear View Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[RearView] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_cameraFOVScansResult[General], 6);

			m_generalFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGeneralFOV = 1.0f / s_instance_->m_aspectRatioScale;
				FPU::FDIVR(s_instance_->m_newGeneralFOV);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[Chase], 6);

			m_chaseFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Chase], [](SafetyHookContext& ctx)
			{
				s_instance_->VehicleFOVMidHook(ctx.ebx + 0x28, ctx.ebx + 0x2C);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[Cockpit], 6);

			m_cockpitFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Cockpit], [](SafetyHookContext& ctx)
			{
				s_instance_->VehicleFOVMidHook(ctx.edi + 0x28, ctx.edi + 0x2C);
			});

			m_newRearViewFOV = m_originalRearViewFOV * m_fovFactor;

			Memory::Write(m_cameraFOVScansResult[RearView] + 1, m_newRearViewFOV);
		}

		if (m_runMultipleInstances == true)
		{
			m_multipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 8B 95 ?? ?? ?? ?? 85 D2 0F 95 C0");
			if (m_multipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_multipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_multipleInstancesCheckScanResult, 6);
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "FF 24 85 ?? ?? ?? ?? A0");
			if (m_skipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				m_jumpTableCases = static_cast<std::uintptr_t>(*reinterpret_cast<const std::uint32_t*>(m_skipIntroVideosScanResult + 3));
				m_exitTarget = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(m_skipIntroVideosScanResult) + 0x1B6);

				for (std::uintptr_t offset = 0x00; offset <= 0x10; offset += sizeof(std::uint32_t))
				{
					Memory::Write(m_jumpTableCases + offset, m_exitTarget);
				}
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalRearViewFOV = 70.0f;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	std::uint8_t* m_resolutionScanResult = nullptr;
	std::uint8_t* m_aspectRatioScanResult = nullptr;
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::uint8_t* m_multipleInstancesCheckScanResult = nullptr;
	std::uint8_t* m_skipIntroVideosScanResult;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_chaseFOVHook{};
	SafetyHookMid m_cockpitFOVHook{};

	uintptr_t m_currentWidthAddress = 0;
	uintptr_t m_currentHeightAddress = 0;

	float m_currentVehicleFOV1 = 0.0f;
	float m_currentVehicleFOV2 = 0.0f;
	float m_newGeneralFOV = 0.0f;
	float m_newVehicleFOV1 = 0.0f;
	float m_newVehicleFOV2 = 0.0f;
	float m_newRearViewFOV = 0.0f;

	uintptr_t m_jumpTableCases = 0;
	uint32_t m_exitTarget = 0;

	enum CameraFOVInstructionsIndex
	{
		General,
		Chase,
		Cockpit,
		RearView
	};

	enum SkipIntroVideosInstructionsIndex
	{
		EmpireLogo,
		EutechnyxLogo
	};

	void ResolutionMidHook(SafetyHookContext& ctx)
	{
		m_newResX = Memory::ReadMem(m_currentWidthAddress);
		m_newResY = Memory::ReadMem(m_currentHeightAddress);
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
	}

	void VehicleFOVMidHook(uintptr_t fovAddress1, uintptr_t fovAddress2)
	{
		m_currentVehicleFOV1 = Memory::ReadMem(fovAddress1);
		m_currentVehicleFOV2 = Memory::ReadMem(fovAddress2);
		m_newVehicleFOV1 = m_currentVehicleFOV1 * m_fovFactor;
		m_newVehicleFOV2 = m_currentVehicleFOV2 * m_fovFactor;
		FPU::FMUL(m_newVehicleFOV1);
		FPU::FLD(m_newVehicleFOV2);
	}

	inline static BigMuthaTruckersFix* s_instance_ = nullptr;
};

static std::unique_ptr<BigMuthaTruckersFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
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