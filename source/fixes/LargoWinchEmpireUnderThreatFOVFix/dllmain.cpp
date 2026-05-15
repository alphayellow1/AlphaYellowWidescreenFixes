#include "..\..\common\FixBase.hpp"

class LargoWinchFix final : public FixBase
{
public:
	explicit LargoWinchFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~LargoWinchFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "LargoWinchEmpireUnderThreatFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "Largo Winch: Empire Under Threat";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "LargoWinch.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 8B 4C 24 ?? A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 90 90 90 A1");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? 8B 4C 24 ?? D9 58 ?? D9 44 24");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 4);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newAspectRatio2 = Maths::CalculateNewHFOV_RadBased(fCurrentAspectRatio, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 41 ?? D9 54 24", "89 48 ?? C3 90 90 90 90 90 90 90 90 90 90 A0");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, Gameplay, Cutscenes, 0, 3);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->GameplayFOVMidHook(ctx);
			});

			m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes], [](SafetyHookContext& ctx)
			{
				s_instance_->CutscenesFOVMidHook(ctx);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_cutscenesFOVHook{};

	float m_newGameplayFOV = 0.0f;
	float m_newCutscenesFOV = 0.0f;

	enum CameraFOVInstructionsIndices
	{
		Gameplay,
		Cutscenes
	};

	void GameplayFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentGameplayFOV = Memory::ReadMem(ctx.ecx + 0x5C);
		m_newGameplayFOV = fCurrentGameplayFOV * m_fovFactor;
		FPU::FLD(m_newGameplayFOV);
	}

	void CutscenesFOVMidHook(SafetyHookContext& ctx)
	{
		const float& fCurrentCutscenesFOV = Memory::ReadRegister(ctx.ecx);
		m_newCutscenesFOV = fCurrentCutscenesFOV / m_fovFactor;
		*reinterpret_cast<float*>(ctx.eax + 0x5C) = m_newCutscenesFOV;
	}

	inline static LargoWinchFix* s_instance_ = nullptr;
};

static std::unique_ptr<LargoWinchFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<LargoWinchFix>(hModule);
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