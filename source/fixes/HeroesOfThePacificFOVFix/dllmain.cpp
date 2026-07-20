#include "..\..\common\FixBase.hpp"

class HeroesOfThePacificFix final : public FixBase
{
public:
	explicit HeroesOfThePacificFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~HeroesOfThePacificFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "HeroesOfThePacificFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Heroes of the Pacific";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Heroes.exe");
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
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 08 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? 8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? 8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? 8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? 8B 40");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {}+{:x}", ExeName().c_str(), ResolutionScanResult - reinterpret_cast<std::uint8_t*>(ExeModule()));

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.eax);
				int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::info("Cannot locate the resolution instruction memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D8 30 D8 48 ?? D8 F9 D9 1C ?? D8 70 ?? D9 5C 24 ?? 74 ?? 8D 04 ?? 50 51 E8 ?? ?? ?? ?? 83 C4 ?? 83 C4 ?? C2 ?? ?? CC CC CC CC CC",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? 74 ?? 8D 4C 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 83 C4 ?? 5F");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Briefing Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[BriefingAR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Briefing Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[BriefingAR2] - (std::uint8_t*)ExeModule());			
			
			Memory::WriteNOPs(AspectRatioScansResult, GameplayAR, BriefingAR2, 0, 6);

			m_gameplayAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[GameplayAR], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGameplayAspectRatio = 1.0f / s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newGameplayAspectRatio);
			});			

			m_briefingScreenAspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[BriefingAR1], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newBriefingScreenAspectRatio = 0.75f / s_instance_->m_aspectRatioScale;
				FPU::FMUL(s_instance_->m_newBriefingScreenAspectRatio);
			});

			m_briefingScreenAspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[BriefingAR2], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newBriefingScreenAspectRatio = 0.75f / s_instance_->m_aspectRatioScale;
				FPU::FMUL(s_instance_->m_newBriefingScreenAspectRatio);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 51 ?? A3 ?? ?? ?? ?? 8B 41", "DD 05 ?? ?? ?? ?? A1", "DD 05 ?? ?? ?? ?? D9 F2");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GameplayFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Briefing Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[BriefingFOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Briefing Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[BriefingFOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[GameplayFOV], 3);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[GameplayFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayCameraFOV = Memory::ReadMem(ctx.ecx + 0x8);
				s_instance_->m_newGameplayCameraFOV = fCurrentGameplayCameraFOV * s_instance_->m_fovFactor;
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newGameplayCameraFOV);
			});

			m_briefingScreenFOV1Address = Memory::GetPointerFromAddress(CameraFOVScansResult[BriefingFOV1] + 2, Memory::PointerMode::Absolute);
			m_briefingScreenFOV2Address = Memory::GetPointerFromAddress(CameraFOVScansResult[BriefingFOV2] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult, BriefingFOV1, BriefingFOV2, 0, 6);

			m_briefingScreenFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[BriefingFOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->BriefingScreenFOVsMidHook(s_instance_->m_briefingScreenFOV1Address);
			});

			m_briefingScreenFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[BriefingFOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->BriefingScreenFOVsMidHook(s_instance_->m_briefingScreenFOV2Address);
			});
		}		

		if (m_runMultipleInstances == true)
		{
			auto MultipleInstancesCheckScansResult = Memory::PatternScan(ExeModule(), "74 ?? 50 FF 15 ?? ?? ?? ?? E9", "0F 84 ?? ?? ?? ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24");
			if (Memory::AreAllSignaturesValid(MultipleInstancesCheckScansResult) == true)
			{
				spdlog::info("Multiple Instance Check Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), MultipleInstancesCheckScansResult[MutexName] - (std::uint8_t*)ExeModule());
				spdlog::info("Multiple Instance Check Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), MultipleInstancesCheckScansResult[WindowName] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(MultipleInstancesCheckScansResult[MutexName], "\xEB");
				Memory::WriteNOPs(MultipleInstancesCheckScansResult[WindowName], 6);
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideoScansResult = Memory::PatternScan(ExeModule(), "76 ?? D9 05 ?? ?? ?? ?? D9 7C 24",
			"68 ?? ?? ?? ?? 8D 4C 24 ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24 ?? 8B DE E8 ?? ?? ?? ?? 83 C4 ?? 8D 4C 24 ?? E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? EB");
			if (Memory::AreAllSignaturesValid(SkipIntroVideoScansResult) == true)
			{
				spdlog::info("Legal Screen Wait Loop Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScansResult[LegalScreenWait] - (std::uint8_t*)ExeModule());
				spdlog::info("Boot Sequence String Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScansResult[BootSequence] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideoScansResult[LegalScreenWait], "\xEB");
				Memory::Write(SkipIntroVideoScansResult[BootSequence] + 1, m_newGameState);
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	const char* m_newGameState = "mainmenu";

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_gameplayAspectRatioHook{};
	SafetyHookMid m_briefingScreenAspectRatio1Hook{};
	SafetyHookMid m_briefingScreenAspectRatio2Hook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_briefingScreenFOV1Hook{};
	SafetyHookMid m_briefingScreenFOV2Hook{};

	float m_newGameplayAspectRatio = 0.0f;
	float m_newBriefingScreenAspectRatio = 0.0f;
	float m_newGameplayCameraFOV = 0.0f;
	double m_newBriefingScreenCameraFOV = 0.0;

	uintptr_t m_briefingScreenFOV1Address = 0;
	uintptr_t m_briefingScreenFOV2Address = 0;	

	enum AspectRatioInstructionsIndices
	{
		GameplayAR,
		BriefingAR1,
		BriefingAR2
	};

	enum CameraFOVInstructionsIndices
	{
		GameplayFOV,
		BriefingFOV1,
		BriefingFOV2
	};

	enum MultipleInstancesChecksIndices
	{
		MutexName,
		WindowName
	};

	enum SkipIntroVideosIndices
	{
		LegalScreenWait,
		BootSequence
	};

	void BriefingScreenFOVsMidHook(uintptr_t fovAddress)
	{
		double& dCurrentBriefingScreenCameraFOV = Memory::ReadMem(fovAddress);
		m_newBriefingScreenCameraFOV = Maths::CalculateNewFOV_RadBased(dCurrentBriefingScreenCameraFOV, m_aspectRatioScale, Maths::AngleMode::HalfAngle);
		FPU::FLD(m_newBriefingScreenCameraFOV);
	}

	inline static HeroesOfThePacificFix* s_instance_ = nullptr;
};

static std::unique_ptr<HeroesOfThePacificFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<HeroesOfThePacificFix>(hModule);
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