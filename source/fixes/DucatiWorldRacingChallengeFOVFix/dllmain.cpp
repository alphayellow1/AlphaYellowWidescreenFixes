#include "..\..\common\FixBase.hpp"

class DucatiWRCFix final : public FixBase
{
public:
	explicit DucatiWRCFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DucatiWRCFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DucatiWorldRacingChallengeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Ducati World: Racing Challenge";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ducati.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "66 8B 88 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 66 8B 90 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? B8");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->UpdateAR(ctx.eax + 0x117C, ctx.eax + 0x117E);
				s_instance_->WriteStaticFOVs();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 66 89 0D ?? ?? ?? ?? 66 8B 90", "68 ?? ?? ?? ?? 8B D0",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 5F 5E 5D 81 C4", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 C4 ?? 85 C0 0F 84",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0B", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 33 C0");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "0F 94 C0 C3 90 90 90 90 90 90 90 90 90 90 90 90 90 90");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(RunMultipleInstancesCheckScanResult, 3);
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr int16_t m_originalCameraFOV1 = 268;
	static constexpr int16_t m_originalCameraFOV2 = 214;
	static constexpr int16_t m_originalCameraFOV3 = 150;

	SafetyHookMid m_resolutionHook{};

	std::vector<std::uint8_t*> CameraFOVScansResult;

	bool m_runMultipleInstances = false;

	int16_t m_newCameraFOV1 = 0;
	int16_t m_newCameraFOV2 = 0;
	int16_t m_newCameraFOV3 = 0;

	int16_t m_newResX = 0;
	int16_t m_newResY = 0;

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6
	};

	void UpdateAR(uintptr_t widthAddress, uintptr_t heightAddress)
	{
		m_newResX = Memory::ReadMem(widthAddress);
		m_newResY = Memory::ReadMem(heightAddress);
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / s_instance_->m_oldAspectRatio;
	}

	void WriteStaticFOVs()
	{
		m_newCameraFOV1 = (int16_t)(m_originalCameraFOV1 / m_aspectRatioScale);
		m_newCameraFOV2 = (int16_t)(m_originalCameraFOV2 / m_aspectRatioScale);
		m_newCameraFOV3 = (int16_t)(m_originalCameraFOV3 / m_aspectRatioScale);

		Memory::Write(CameraFOVScansResult[FOV1] + 1, m_newCameraFOV1);
		Memory::Write(CameraFOVScansResult[FOV2] + 1, m_newCameraFOV2);
		Memory::Write(CameraFOVScansResult[FOV3] + 1, m_newCameraFOV1);
		Memory::Write(CameraFOVScansResult[FOV4] + 1, m_newCameraFOV1);
		Memory::Write(CameraFOVScansResult[FOV5] + 1, (int16_t)(m_newCameraFOV1 / m_fovFactor));
		Memory::Write(CameraFOVScansResult[FOV6] + 1, m_newCameraFOV3);
	}

	inline static DucatiWRCFix* s_instance_ = nullptr;
};

static std::unique_ptr<DucatiWRCFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<DucatiWRCFix>(hModule);
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