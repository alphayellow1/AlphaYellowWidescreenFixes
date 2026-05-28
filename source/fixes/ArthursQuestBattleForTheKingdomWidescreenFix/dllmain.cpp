#include "..\..\common\FixBase.hpp"

class ArthursQuestFix final : public FixBase
{
public:
	explicit ArthursQuestFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ArthursQuestFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ArthursQuestBattleForTheKingdomWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "Arthur's Quest: Battle for the Kingdom";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ArthursQuest.exe") || 
		Util::stringcmp_caseless(exeName, "lithtech.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "ArthursQuest.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "72 ?? 8B 86 ?? ?? ?? ?? 3D");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 2);
				Memory::WriteNOPs(ResolutionListUnlockScanResult + 13, 2);
				Memory::WriteNOPs(ResolutionListUnlockScanResult + 33, 2);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "lithtech.exe"))
		{
			m_dllModule2 = Memory::GetHandle("CShell.dll");
			m_dllModule2Name = Memory::GetModuleName(m_dllModule2);

			auto ResolutionScansResult = Memory::PatternScan(m_dllModule2, "0F 82 ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? 3D", ExeModule(), "8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? FF 90");
			if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", m_dllModule2Name.c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)m_dllModule2);
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 6);
				Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 17, 6);
				Memory::PatchBytes(ResolutionScansResult[ResListUnlock] + 41, "\xEB");

				m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.eax + 0x3C);
					int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x40);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				});
			}

			auto CameraFOVScansResult = Memory::PatternScan(m_dllModule2, "DB 44 24 ?? 51 8B 0D ?? ?? ?? ?? D9 1C 24",
			"DB 44 24 ?? 6A ?? 51 D9 1C 24 DB 44 24 ?? 51 8B 0D ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? D9 1C 24 52 E8 ?? ?? ?? ?? A1",
			"DB 44 24 ?? 51 8B CF");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Gameplay HFOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), CameraFOVScansResult[GameplayHFOV] - (std::uint8_t*)m_dllModule2);
				spdlog::info("Gameplay VFOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), CameraFOVScansResult[GameplayVFOV] - (std::uint8_t*)m_dllModule2);
				spdlog::info("Cutscenes HFOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), CameraFOVScansResult[CutscenesHFOV] - (std::uint8_t*)m_dllModule2);

				Memory::WriteNOPs(CameraFOVScansResult[GameplayHFOV], 4);

				m_gameplayHFOVHook = safetyhook::create_mid(CameraFOVScansResult[GameplayHFOV], [](SafetyHookContext& ctx)
				{
					uint32_t& iCurrentGameplayHFOV = Memory::ReadMem(ctx.esp + 0x1C);
					s_instance_->m_newGameplayHFOV = Maths::CalculateNewHFOV_DegBased((float)iCurrentGameplayHFOV, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
					FPU::FILD((uint32_t)s_instance_->m_newGameplayHFOV);
				});

				Memory::WriteNOPs(CameraFOVScansResult[GameplayVFOV], 4);

				m_gameplayVFOVHook = safetyhook::create_mid(CameraFOVScansResult[GameplayVFOV], [](SafetyHookContext& ctx)
				{
					uint32_t& iCurrentGameplayVFOV = Memory::ReadMem(ctx.esp + 0x18);
					s_instance_->m_newGameplayVFOV = Maths::CalculateNewVFOV_DegBased((float)iCurrentGameplayVFOV, s_instance_->m_fovFactor);
					FPU::FILD((uint32_t)s_instance_->m_newGameplayVFOV);
				});

				Memory::WriteNOPs(CameraFOVScansResult[CutscenesHFOV], 4);

				m_cutscenesHFOVHook = safetyhook::create_mid(CameraFOVScansResult[CutscenesHFOV], [](SafetyHookContext& ctx)
				{
					uint32_t& iCurrentCutscenesHFOV = Memory::ReadMem(ctx.esp + 0x20);
					s_instance_->m_newCutscenesHFOV = Maths::CalculateNewHFOV_DegBased((float)iCurrentCutscenesHFOV, s_instance_->m_aspectRatioScale);
					FPU::FILD((uint32_t)s_instance_->m_newCutscenesHFOV);
				});
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	HMODULE m_dllModule2 = nullptr;
	std::string m_dllModule2Name = "";

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_gameplayHFOVHook{};
	SafetyHookMid m_gameplayVFOVHook{};
	SafetyHookMid m_cutscenesHFOVHook{};

	float m_newGameplayHFOV = 0.0f;
	float m_newGameplayVFOV = 0.0f;
	float m_newCutscenesHFOV = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum CameraFOVInstructionsIndex
	{
		GameplayHFOV,
		GameplayVFOV,
		CutscenesHFOV
	};

	inline static ArthursQuestFix* s_instance_ = nullptr;
};

static std::unique_ptr<ArthursQuestFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ArthursQuestFix>(hModule);
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