#include "..\..\common\FixBase.hpp"

class TMNT2Fix final : public FixBase
{
public:
	explicit TMNT2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TMNT2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TeenageMutantNinjaTurtles2BattleNexusWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Teenage Mutant Turtles 2: Battle Nexus";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "TMNT2.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionInstructionsScansResult = Memory::PatternScan(ExeModule(), "8B 08 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? C7 05", "89 46 ?? 8B 3D", "A3 ?? ?? ?? ?? 8B 11 FF 52");
		if (Memory::AreAllSignaturesValid(ResolutionInstructionsScansResult) == true)
		{
			spdlog::info("Renderer Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionInstructionsScansResult[Renderer] - (std::uint8_t*)ExeModule());
			spdlog::info("Viewport Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionInstructionsScansResult[Viewport1] - (std::uint8_t*)ExeModule());
			spdlog::info("Viewport Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionInstructionsScansResult[Viewport2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Renderer], 2);

			m_rendererWidthHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Renderer], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Renderer] + 8, 3);

			m_rendererHeightHook = safetyhook::create_mid(ResolutionInstructionsScansResult[Renderer] + 8, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport1], 3);

			m_ViewportWidth1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport1], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0xC) = s_instance_->m_newResX;
			});

			Memory::WriteNOPs(ResolutionInstructionsScansResult[Viewport1] + 33, 3);

			m_ViewportHeight1Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport1] + 33, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.esi + 0x10) = s_instance_->m_newResY;
			});

			m_ViewportWidth2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport2], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			m_ViewportHeight2Hook = safetyhook::create_mid(ResolutionInstructionsScansResult[Viewport2] + 16, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 51 D9 5C 24 ?? 8B 53", "D8 0D ?? ?? ?? ?? 51 D9 5C 24 ?? 8B 50");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CutscenesAR] - (std::uint8_t*)ExeModule());

			m_newAspectRatio2 = 1.0f / m_newAspectRatio;

			Memory::WriteNOPs(AspectRatioScansResult, GameplayAR, CutscenesAR, 0, 6);

			m_gameplayAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[GameplayAR], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioMidHook();
			});

			m_cutscenesAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[CutscenesAR], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioMidHook();
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D8 63", "D9 46 ?? 8B 46 ?? 85 C0");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GameplayFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[CutscenesFOV] - (std::uint8_t*)ExeModule());

			m_cameraFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[GameplayFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[GameplayFOV], 6);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[GameplayFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayFOV = Memory::ReadMem(s_instance_->m_cameraFOVAddress);
				s_instance_->m_newGameplayFOV = fCurrentGameplayFOV * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newGameplayFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[CutscenesFOV], 3);

			m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVScansResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV = Memory::ReadMem(ctx.esi + 0x24);
				s_instance_->m_newCutscenesFOV = fCurrentCutscenesFOV * s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newCutscenesFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_rendererWidthHook{};
	SafetyHookMid m_rendererHeightHook{};
	SafetyHookMid m_ViewportWidth1Hook{};
	SafetyHookMid m_ViewportHeight1Hook{};
	SafetyHookMid m_ViewportWidth2Hook{};
	SafetyHookMid m_ViewportHeight2Hook{};
	SafetyHookMid m_gameplayAspectRatioHook{};
	SafetyHookMid m_cutscenesAspectRatioHook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_cutscenesFOVHook{};

	enum ResolutionInstructionsIndex
	{
		Renderer,
		Viewport1,
		Viewport2
	};

	enum AspectRatioInstructionsIndex
	{
		GameplayAR,
		CutscenesAR
	};

	enum CameraFOVInsructionsIndex
	{
		GameplayFOV,
		CutscenesFOV
	};

	uintptr_t m_cameraFOVAddress = 0;

	float m_newGameplayFOV = 0.0f;
	float m_newCutscenesFOV = 0.0f;

	void AspectRatioMidHook()
	{
		FPU::FMUL(m_newAspectRatio2);
	}

	inline static TMNT2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<TMNT2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<TMNT2Fix>(hModule);
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