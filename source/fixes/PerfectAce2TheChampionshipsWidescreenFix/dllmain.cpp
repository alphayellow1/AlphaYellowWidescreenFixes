#include "..\..\common\FixBase.hpp"

class PerfectAce2Fix final : public FixBase
{
public:
	explicit PerfectAce2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PerfectAce2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "PerfectAce2TheChampionshipsWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Perfect Ace 2: The Championships";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ACEPC.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionInstructionsScansResult = Memory::PatternScan(ExeModule(), "0F 8F ?? ?? ?? ?? 81 FA", "8B 0F 89 0D");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionInstructionsScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionInstructionsScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock], 6);
			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 12, 6);
			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 28, 6);
			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 53, 6);
			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 70, 2);
			Memory::WriteNOPs(ResolutionInstructionsScansResult[ResListUnlock] + 84, 2);

			m_resolutionHook = safetyhook::create_mid(ResolutionInstructionsScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentResX = Memory::ReadMem(ctx.edi);
				int& iCurrentResY = Memory::ReadMem(ctx.edi + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 B4 81", "8D 4C 24 ?? 51 56 E8 ?? ?? ?? ?? 8D 54 24 ?? 52 56 E8 ?? ?? ?? ?? 8B 44 24");

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 8C 81 ?? ?? ?? ?? 52", "D8 8C 81 ?? ?? ?? ?? D8 B4 81", "8B 4C 24 ?? 53 51 B9", "D9 86 ?? ?? ?? ?? 8B 85");

		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CutscenesAR] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[GameplayAR], 7);

			m_gameplayAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[GameplayAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = Memory::ReadMem(ctx.ecx + ctx.eax * 0x4 + 0x11C);
				s_instance_->m_newAspectRatio2 = fCurrentAspectRatio * s_instance_->m_aspectRatioScale;
				FPU::FDIV(s_instance_->m_newAspectRatio2);
			});

			m_cutscenesAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[CutscenesAR], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesAspectRatio = Memory::ReadMem(ctx.esp + 0x14);
				fCurrentCutscenesAspectRatio = fCurrentCutscenesAspectRatio * s_instance_->m_aspectRatioScale;
			});
		}

		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[HFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[VFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[MainMenuFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[CutscenesFOV] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[HFOV], 7);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScansResult[HFOV], GameplayFOVMidHook);

			Memory::WriteNOPs(CameraFOVScansResult[VFOV], 7);

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScansResult[VFOV], GameplayFOVMidHook);

			Memory::WriteNOPs(CameraFOVScansResult[MainMenuFOV], 4);

			m_mainMenuFOVHook = safetyhook::create_mid(CameraFOVScansResult[MainMenuFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentMainMenuFOV = Memory::ReadMem(ctx.esp + 0x14);
				s_instance_->m_newMainMenuFOV = fCurrentMainMenuFOV / s_instance_->m_fovFactor;
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newMainMenuFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[CutscenesFOV], 6);

			m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVScansResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV = Memory::ReadMem(ctx.esi + 0xA4);
				s_instance_->m_newCutscenesFOV = (fCurrentCutscenesFOV / s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCutscenesFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_gameplayAspectRatioHook{};
	SafetyHookMid m_cutscenesAspectRatioHook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};
	SafetyHookMid m_mainMenuFOVHook{};
	SafetyHookMid m_cutscenesFOVHook{};

	float m_newGameplayFOV = 0.0f;
	float m_newMainMenuFOV = 0.0f;
	float m_newCutscenesFOV = 0.0f;

	enum ResolutionInstructionsIndices
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum AspectRatioInstructionsIndices
	{
		GameplayAR,
		CutscenesAR
	};

	enum CameraFOVInstructionsIndices
	{
		HFOV,
		VFOV,
		MainMenuFOV,
		CutscenesFOV
	};

	static void GameplayFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentGameplayFOV = Memory::ReadMem(ctx.ecx + ctx.eax * 0x4 + 0xC0);
		s_instance_->m_newGameplayFOV = fCurrentGameplayFOV * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
		FPU::FMUL(s_instance_->m_newGameplayFOV);
	}

	inline static PerfectAce2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<PerfectAce2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<PerfectAce2Fix>(hModule);
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