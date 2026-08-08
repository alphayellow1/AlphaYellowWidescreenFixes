#include "..\..\common\FixBase.hpp"

class NinaAgentChroniclesFix final : public FixBase
{
public:
	explicit NinaAgentChroniclesFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~NinaAgentChroniclesFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "NinaAgentChroniclesWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.8";
	}

	const char* TargetName() const override
	{
		return "Nina Agent Chronicles";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "lithtech.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_clientShellDllModule = Memory::GetHandle("cshell.dll");
		m_clientShellDllModuleName = Memory::GetModuleName(m_clientShellDllModule);

		m_resolutionScansResult = Memory::PatternScan(m_clientShellDllModule, "0F 82 ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? 3D",
		ExeModule(), "8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? FF 90");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_resolutionScansResult[ListUnlock] - (std::uint8_t*)m_clientShellDllModule);
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_resolutionScansResult[ListUnlock], 6);
			Memory::WriteNOPs(m_resolutionScansResult[ListUnlock] + 17, 6);

			if (*(m_resolutionScansResult[ListUnlock] + 23) == 0x83)
			{
				Memory::WriteNOPs(m_resolutionScansResult[ListUnlock] + 30, 6);
				Memory::PatchBytes(m_resolutionScansResult[ListUnlock] + 54, "\xEB");
			}
			else
			{
				Memory::PatchBytes(m_resolutionScansResult[ListUnlock] + 41, "\xEB");
			}

			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.eax + 0x3C);
				int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x40);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		m_cameraFOVScansResult = Memory::PatternScan(m_clientShellDllModule, "DB 44 24 ?? 8B 15", "DB 44 24 ?? 51 8B CD", "D9 44 24 ?? D8 0D ?? ?? ?? ?? 51 8B 87",
		"D8 86 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 8B 87 ?? ?? ?? ?? F6 C4 ?? 74 ?? D9 87");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay FOV Instructions Scan: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Gameplay] - (std::uint8_t*)m_clientShellDllModule);
			spdlog::info("Cutscenes HFOV Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Cutscenes] - (std::uint8_t*)m_clientShellDllModule);
			spdlog::info("Menu VFOV Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Menu] - (std::uint8_t*)m_clientShellDllModule);
			spdlog::info("Death Camera HFOV Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Death] - (std::uint8_t*)m_clientShellDllModule);

			Memory::WriteNOPs(m_cameraFOVScansResult[Gameplay] + 22, 4);

			m_gameplayHFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Gameplay] + 22, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGameplayHFOV = Memory::ReadMem(ctx.esp + 0x1C);
				s_instance_->m_currentGameplayVFOV = Memory::ReadMem(ctx.esp + 0x20);				

				if (s_instance_->m_currentGameplayHFOV == s_instance_->m_defaultCameraHFOV && s_instance_->m_currentGameplayVFOV == s_instance_->m_defaultCameraVFOV)
				{
					s_instance_->m_newGameplayHFOV = (int)Maths::CalculateNewHFOV_DegBased((float)s_instance_->m_currentGameplayHFOV, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor); // Hipfire HFOV
				}
				else
				{
					s_instance_->m_newGameplayHFOV = (int)Maths::CalculateNewHFOV_DegBased((float)s_instance_->m_currentGameplayHFOV, s_instance_->m_aspectRatioScale); // All the other HFOVs during gameplay
				}

				FPU::FILD(s_instance_->m_newGameplayHFOV);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[Gameplay], 4);

			m_gameplayVFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGameplayHFOV = Memory::ReadMem(ctx.esp + 0x14);
				s_instance_->m_currentGameplayVFOV = Memory::ReadMem(ctx.esp + 0x18);

				if (s_instance_->m_currentGameplayHFOV == s_instance_->m_defaultCameraHFOV && s_instance_->m_currentGameplayVFOV == s_instance_->m_defaultCameraVFOV)
				{
					s_instance_->m_newGameplayVFOV = (int)Maths::CalculateNewVFOV_DegBased((float)s_instance_->m_currentGameplayVFOV, s_instance_->m_fovFactor);
				}
				else
				{
					s_instance_->m_newGameplayVFOV = s_instance_->m_currentGameplayVFOV;
				}

				FPU::FILD(s_instance_->m_newGameplayVFOV);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[Cutscenes], 4);

			m_cutscenesHFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Cutscenes], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCutscenesHFOV = Memory::ReadMem(ctx.esp + 0x4C);
				s_instance_->m_newCutscenesHFOV = (int)Maths::CalculateNewHFOV_DegBased((float)s_instance_->m_currentCutscenesHFOV, s_instance_->m_aspectRatioScale);
				FPU::FILD(s_instance_->m_newCutscenesHFOV);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[Menu], 4);

			m_menuVFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Menu], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentMenuVFOV = Memory::ReadMem(ctx.esp + 0xC);
				s_instance_->m_newMenuVFOV = s_instance_->m_currentMenuVFOV / s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newMenuVFOV);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[Death], 6);

			m_deathHFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Death], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentDeathHFOV = Memory::ReadMem(ctx.esi + 0x1B4);

				if (s_instance_->m_currentDeathHFOV == m_defaultCameraHFOV2)
				{
					s_instance_->m_newDeathHFOV = Maths::CalculateNewHFOV_RadBased(s_instance_->m_currentDeathHFOV, s_instance_->m_aspectRatioScale);
				}
				else
				{
					s_instance_->m_newDeathHFOV = s_instance_->m_currentDeathHFOV;
				}
				
				FPU::FADD(s_instance_->m_newDeathHFOV);
			});
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScansResult = Memory::PatternScan(m_clientShellDllModule, "8B 86 CC 42 03 00 3B C7 A1 ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? 3B C7 0F 85 ?? ?? ?? ??",
			"A1 ?? ?? ?? ?? 25 FF 00 00 00 2B C3 74 ?? 48 75 ?? 6A 01 8B CE");
			if (Memory::AreAllSignaturesValid(m_skipIntroVideosScansResult) == true)
			{
				spdlog::info("Lemon Interactive Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_skipIntroVideosScansResult[LemonLogo] - (std::uint8_t*)m_clientShellDllModule);
				spdlog::info("Detalion Logo Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_skipIntroVideosScansResult[DetalionLogo] - (std::uint8_t*)m_clientShellDllModule);

				Memory::PatchBytes(m_skipIntroVideosScansResult[LemonLogo], "\xE9\xA5\x00\x00\x00");
				Memory::PatchBytes(m_skipIntroVideosScansResult[DetalionLogo], "\xB8\x01\x00\x00\x00");
			}
		}
	}

private:
	HMODULE m_clientShellDllModule = nullptr;
	std::string m_clientShellDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr int m_defaultCameraHFOV = 90;
	static constexpr int m_defaultCameraVFOV = 78;
	static constexpr float m_defaultCameraHFOV2 = 1.570796371f;

	bool m_skipIntroVideos = false;
	
	int m_currentGameplayHFOV = 0;
	int m_newGameplayHFOV = 0;
	int m_currentGameplayVFOV = 0;
	int m_newGameplayVFOV = 0;
	int m_currentCutscenesHFOV = 0;
	int m_newCutscenesHFOV = 0;
	float m_currentMenuVFOV = 0.0f;
	float m_newMenuVFOV = 0.0f;
	float m_currentDeathHFOV = 0.0f;
	float m_newDeathHFOV = 0.0f;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::vector<std::uint8_t*> m_skipIntroVideosScansResult{};

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_gameplayHFOVHook{};
	SafetyHookMid m_gameplayVFOVHook{};
	SafetyHookMid m_cutscenesHFOVHook{};
	SafetyHookMid m_menuVFOVHook{};
	SafetyHookMid m_deathHFOVHook{};

	enum ResolutionsInstructionsIndices
	{
		ListUnlock,
		WidthHeight
	};

	enum CameraFOVInstructionsIndices
	{
		Gameplay,
		Cutscenes,
		Menu,
		Death
	};

	enum SkipIntroVideosInstructionsIndices
	{
		LemonLogo,
		DetalionLogo
	};

	inline static NinaAgentChroniclesFix* s_instance_ = nullptr;
};

static std::unique_ptr<NinaAgentChroniclesFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<NinaAgentChroniclesFix>(hModule);
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