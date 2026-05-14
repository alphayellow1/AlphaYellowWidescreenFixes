#include "..\..\common\FixBase.hpp"

class SecretAgentBarbieFix final : public FixBase
{
public:
	explicit SecretAgentBarbieFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SecretAgentBarbieFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SecretAgentBarbieWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Secret Agent Barbie";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "SecretAgent.exe");
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

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? E8",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 6A ?? E8 ?? ?? ?? ?? 8B 4C 24", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A0",
		"C7 46 ?? ?? ?? ?? ?? 89 46 ?? 89 46 ?? 8A 46 ?? C7 46 ?? ?? ?? ?? ?? 24 ?? 88 5E", "C7 46 ?? ?? ?? ?? ?? 89 46 ?? 89 46 ?? 8A 46 ?? C7 46 ?? ?? ?? ?? ?? 24 ?? C7 46",
		"68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 6A ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 6A ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? 6A ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 6A ?? 6A ?? 68 ?? ?? ?? ?? 6A ?? 6A ?? E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 6A",
		"68 ?? ?? ?? ?? 8B D9 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 4 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res4] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 5 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res5] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 6 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res6] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 7 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res7] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 8 Scan: Address is {:s}+{:x}", ExeName().c_str(), static_cast<std::uintptr_t>(0x004E0E98) - (std::uintptr_t)ExeModule());
			spdlog::info("Resolution Instructions 9 Scan: Address is {:s}+{:x}", ExeName().c_str(), static_cast<std::uintptr_t>(0x004F47BD) - (std::uintptr_t)ExeModule());

			Memory::Write(ResolutionScansResult[Res1] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res1] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Res2] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res2] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Res3] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Res3] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Res4] + 3, m_newResX);
			Memory::Write(ResolutionScansResult[Res4] + 19, m_newResY);
			Memory::Write(ResolutionScansResult[Res5] + 3, m_newResX);
			Memory::Write(ResolutionScansResult[Res5] + 19, m_newResY);
			Memory::Write(ResolutionScansResult[Res6] + 1, m_newResX);
			Memory::Write(ResolutionScansResult[Res6] + 29, m_newResY);
			Memory::Write(ResolutionScansResult[Res7] + 8, m_newResX);
			Memory::Write(ResolutionScansResult[Res7] + 1, m_newResY);
			
			Memory::Write((uint8_t*)0x004E0E98 + 1, m_newResX);
			Memory::Write((uint8_t*)0x004E0E98 + 20, m_newResY);
			Memory::Write((uint8_t*)0x004F47BD + 8, m_newResX);
			Memory::Write((uint8_t*)0x004F47BD + 1, m_newResY);
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 5C 24",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? E8");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Outfit Selection Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[OutfitSelectionAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());

			m_newOutfitSelectionAR = m_originalOutfitSelectionAR * m_aspectRatioScale;

			Memory::Write(AspectRatioScansResult[OutfitSelectionAR] + 1, m_newOutfitSelectionAR);
			Memory::Write(AspectRatioScansResult[AR2] + 1, m_newAspectRatio);
			Memory::Write(AspectRatioScansResult[AR3] + 1, m_newAspectRatio);
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? 8B 54 24", "D9 81 ?? ?? ?? ?? 51 D9 1C 24 E8 ?? ?? ?? ?? 8B 4C 24");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Overall Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Overall] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[Overall], 4);
			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 6);

			m_overallFOVHook = safetyhook::create_mid(CameraFOVScansResult[Overall], [](SafetyHookContext& ctx)
			{
				s_instance_->OverallFOVMidHook(ctx.esp + 0x4);
			});

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->GameplayFOVMidHook(ctx.ecx + 0xB0);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalOutfitSelectionAR = 0.9710000157f;

	float m_newOutfitSelectionAR = 0.0f;

	float m_newOverallCameraFOV = 0.0f;

	float m_gameplayOriginalFOV = 0.0f;
	float m_gameplayModifiedFOV = 0.0f;
	bool m_hasGameplayFOV = false;

	SafetyHookMid m_overallFOVHook{};
	SafetyHookMid m_gameplayFOVHook{};

	enum ResolutionInstructionsIndex
	{
		Res1,
		Res2,
		Res3,
		Res4,
		Res5,
		Res6,
		Res7
	};

	enum AspectRatioInstructionsIndex
	{
		OutfitSelectionAR,
		AR2,
		AR3
	};

	enum CameraFOVInstructionsIndex
	{
		Overall,
		Gameplay
	};

	void OverallFOVMidHook(uintptr_t fovAddress)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(fovAddress);

		m_newOverallCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale);

		FPU::FLD(m_newOverallCameraFOV);
	}

	void GameplayFOVMidHook(uintptr_t fovAddress)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(fovAddress);

		constexpr float epsilon = 0.001f;

		// If the current value is not just our previously modified value,
		// treat it as a new genuine game FOV.
		if (!m_hasGameplayFOV ||
			std::fabs(fCurrentCameraFOV - m_gameplayModifiedFOV) > epsilon)
		{
			m_gameplayOriginalFOV = fCurrentCameraFOV;
			m_hasGameplayFOV = true;
		}

		m_gameplayModifiedFOV = m_gameplayOriginalFOV * m_fovFactor;

		FPU::FLD(m_gameplayModifiedFOV);
	}

	inline static SecretAgentBarbieFix* s_instance_ = nullptr;
};

static std::unique_ptr<SecretAgentBarbieFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<SecretAgentBarbieFix>(hModule);
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