#include "..\..\common\FixBase.hpp"

class BarbieMagicOfPegasusFix final : public FixBase
{
public:
	explicit BarbieMagicOfPegasusFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BarbieMagicOfPegasusFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BarbieAndTheMagicOfPegasusWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Barbie and the Magic of Pegasus";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Barbie Pegasus.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? E8 ?? ?? ?? ?? 50",
		"74 ?? 6A ?? 68 ?? ?? ?? ?? 53 6A", "74 ?? 8B 6C 24 ?? 81 FD", "8B 02 A3 ?? ?? ?? ?? 8B 42 ?? A3 ?? ?? ?? ?? 8B 42");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(ResolutionScansResult[ListUnlock1], "\xE9\x1F\x01\x00\x00\x90");
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2], 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock3], 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock3] + 12, 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock3] + 24, 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock3] + 33, 2);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteARAndFOV();
				s_instance_->m_resolutionHook.disable();
			});
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 50 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 52", "68 ?? ?? ?? ?? 8B F0 A1", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 50 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 50 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 53");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR5] - (std::uint8_t*)ExeModule());
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 50 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15", "68 ?? ?? ?? ?? 51 52 E8 ?? ?? ?? ?? 83 C4 ?? B8",
		"68 ?? ?? ?? ?? 50 56 E8 ?? ?? ?? ?? 68", "68 ?? ?? ?? ?? 51 50 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A", "68 ?? ?? ?? ?? 51 50 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 53");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());			
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.5f;

	SafetyHookMid m_resolutionHook{};

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2,
		AR3,
		AR4,
		AR5
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5
	};	

	std::vector<std::uint8_t*> AspectRatioScansResult;
	std::vector<std::uint8_t*> CameraFOVScansResult;

	void WriteARAndFOV()
	{
		Memory::Write(AspectRatioScansResult, AR1, AR5, 1, m_newAspectRatio);

		m_newCameraFOV = m_originalCameraFOV * m_aspectRatioScale * m_fovFactor;

		Memory::Write(CameraFOVScansResult, FOV1, FOV5, 1, m_newCameraFOV);
	}

	enum ResolutionInstructionsIndex
	{
		ListUnlock1,
		ListUnlock2,
		ListUnlock3,
		WidthHeight
	};

	inline static BarbieMagicOfPegasusFix* s_instance_ = nullptr;
};

static std::unique_ptr<BarbieMagicOfPegasusFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BarbieMagicOfPegasusFix>(hModule);
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