#include "..\..\common\FixBase.hpp"

class BeachVolleyHotSportsFix final : public FixBase
{
public:
	explicit BeachVolleyHotSportsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BeachVolleyHotSportsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BeachVolleyHotSportsFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Beach Volley: Hot Sports";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Game.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "73 ?? EB ?? 8D 95", "8B 08 89 0D ?? ?? ?? ?? 8B 95");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());
			
			Memory::PatchBytes(ResolutionScansResult[ResListUnlock], "\xEB");

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.eax);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_dNewAspectRatio = (double)s_instance_->m_newAspectRatio;
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "DD 05 ?? ?? ?? ?? 83 EC 08 DD 1C 24 8B 45 08 D9 40 18 83 EC 08",
		"D8 35 ?? ?? ?? ?? 8B 4D ?? D9 99 ?? ?? ?? ?? 8B 55 ?? 52 8B 45 ?? 83 C0 ?? 50 8D 4D ?? 51 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 8B 4D ?? 81 C1 ?? ?? ?? ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 6A ?? 8B 4D ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC CC CC 55",
		"DD 05 ?? ?? ?? ?? 83 EC 08 DD 1C 24 8B 55 08 D9 42 18 83 EC 08",
		"D8 35 ?? ?? ?? ?? 8B 4D ?? D9 99 ?? ?? ?? ?? 8B 55 ?? 52 8B 45 ?? 83 C0 ?? 50 8D 4D ?? 51 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 8B 4D ?? 81 C1 ?? ?? ?? ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 8D 45 ?? 50 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4D ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? DD D8 6A ?? 8B 4D ?? 51 8B 55 ?? 81 C2 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC CC CC CC");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult, AR1, AR4, 0, 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_dNewAspectRatio);
			});

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], [](SafetyHookContext& ctx)
			{
				FPU::FDIV(s_instance_->m_newAspectRatio);
			});

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_dNewAspectRatio);
			});

			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], [](SafetyHookContext& ctx)
			{
				FPU::FDIV(s_instance_->m_dNewAspectRatio);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "C7 45 ?? ?? ?? ?? ?? 8D 4D ?? 51 8B 4D ?? E8 ?? ?? ?? ?? 8B E5",
		"D8 4A ?? 8B 85", "D8 48 ?? DE C1 D9 5D");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes1] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes2] - (std::uint8_t*)ExeModule());

			m_newGameplayFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult[Gameplay] + 3, m_newGameplayFOV);

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes1], 3);

			m_cutscenesFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes1], [](SafetyHookContext& ctx)
			{
				s_instance_->CutscenesFOVMidHook(ctx.edx + 0x1C);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes2], 3);

			m_cutscenesFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes2], [](SafetyHookContext& ctx)
			{
				s_instance_->CutscenesFOVMidHook(ctx.eax + 0x50);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 54.43199920654297f;

	double m_dNewAspectRatio = 0.0;
	float m_newGameplayFOV = 0.0f;
	float m_newCutscenesFOV = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_cutscenesFOV1Hook{};
	SafetyHookMid m_cutscenesFOV2Hook{};

	enum ResolutionInstructionsIndex
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3,
		AR4
	};

	enum CameraFOVInstructionsIndex
	{
		Gameplay,
		Cutscenes1,
		Cutscenes2
	};	

	void CutscenesFOVMidHook(uintptr_t FOVAddress)
	{
		float& fCurrentCutscenesFOV = Memory::ReadMem(FOVAddress);

		m_newCutscenesFOV = fCurrentCutscenesFOV * m_fovFactor;

		FPU::FMUL(m_newCutscenesFOV);
	}

	inline static BeachVolleyHotSportsFix* s_instance_ = nullptr;
};

static std::unique_ptr<BeachVolleyHotSportsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BeachVolleyHotSportsFix>(hModule);
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