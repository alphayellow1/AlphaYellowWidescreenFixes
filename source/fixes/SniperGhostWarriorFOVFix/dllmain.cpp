#include "..\..\common\FixBase.hpp"

class SniperGhostWarrior1Fix final : public FixBase
{
public:
	explicit SniperGhostWarrior1Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SniperGhostWarrior1Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SniperGhostWarriorFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Sniper: Ghost Warrior";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Sniper_x86.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_enginex86DllModule = Memory::GetHandle("engine_x86.dll");
		m_enginex86DllModuleName = Memory::GetModuleName(m_enginex86DllModule);

		auto ResolutionScanResult = Memory::PatternScan(m_enginex86DllModule, "89 3D ?? ?? ?? ?? 89 35 ?? ?? ?? ?? 59");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_enginex86DllModuleName.c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadRegister(ctx.edi);
				s_instance_->m_currentHeight = Memory::ReadRegister(ctx.esi);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		// Instructions are located in IBaseCamera::SetFOV and IGSObject::GetStatusEditor
		auto CameraFOVScansResult = Memory::PatternScan(m_enginex86DllModule, "D9 44 24 ?? D8 0D ?? ?? ?? ?? D9 1C 24 FF D2", "D9 83 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? D8 0D");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", m_enginex86DllModuleName.c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)m_enginex86DllModule);
			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", m_enginex86DllModuleName.c_str(), CameraFOVScansResult[Cutscenes] - (std::uint8_t*)m_enginex86DllModule);

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 4);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGameplayFOV = Memory::ReadMem(ctx.esp + 0x10);

				if (s_instance_->m_currentGameplayFOV == 46.799999237061f || s_instance_->m_currentGameplayFOV == 46.799995422363f)
				{
					s_instance_->m_newGameplayFOV = s_instance_->m_currentGameplayFOV * s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newGameplayFOV = s_instance_->m_currentGameplayFOV;
				}

				FPU::FLD(s_instance_->m_newGameplayFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes], 6);

			m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCutscenesFOV = Memory::ReadMem(ctx.ebx + 0x150);

				if (s_instance_->m_newAspectRatio > m_oldAspectRatio)
				{
					s_instance_->m_newCutscenesFOV = Maths::CalculateNewFOV_DegBased(s_instance_->m_currentCutscenesFOV, s_instance_->m_aspectRatioScale);
				}
				else if (s_instance_->m_newAspectRatio <= m_oldAspectRatio)
				{
					s_instance_->m_newCutscenesFOV = s_instance_->m_currentCutscenesFOV;
				}

				FPU::FLD(s_instance_->m_newCutscenesFOV);
			});
		}
	}

private:
	HMODULE m_enginex86DllModule = nullptr;
	std::string m_enginex86DllModuleName = "";

	static constexpr float m_oldAspectRatio = 16.0f / 9.0f;

	int m_currentWidth = 0;
	int m_currentHeight = 0;

	float m_currentGameplayFOV = 0.0f;
	float m_currentCutscenesFOV = 0.0f;
	float m_newGameplayFOV = 0.0f;
	float m_newCutscenesFOV = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_cutscenesFOVHook{};

	enum CameraFOVInstructionsIndex
	{
		Gameplay,
		Cutscenes
	};

	inline static SniperGhostWarrior1Fix* s_instance_ = nullptr;
};

static std::unique_ptr<SniperGhostWarrior1Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<SniperGhostWarrior1Fix>(hModule);
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