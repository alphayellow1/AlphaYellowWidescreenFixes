#include "..\..\common\FixBase.hpp"

class CorvetteEvolutionGTFix final : public FixBase
{
public:
	explicit CorvetteEvolutionGTFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~CorvetteEvolutionGTFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "CorvetteEvolutionGTFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Corvette Evolution GT";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "EGT.exe");
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
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 08 8B 4C 24 0C A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 10 68 ?? ?? ?? ?? 6A 00 50 FF 92 98 00 00 00 A1 ?? ?? ?? ?? 8B 08 50 FF 51 08 A1 ?? ?? ?? ?? 8B 10 68 ?? ?? ?? ?? 50 FF 92 A0 00 00 00 A1 ?? ?? ?? ?? 8B 08 50 FF 51 08 E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ??E8 ?? ?? ?? ??E8 ?? ?? ?? ?? 8B 8C 24 6C 04 00 00");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - reinterpret_cast<std::uint8_t*>(ExeModule()));

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x8);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0xC);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? 77 ?? C7 44 24", "C7 44 24 ?? ?? ?? ?? ?? 8B 54 24 ?? 8B 46",
		"C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? A1", "8B 54 24 ?? D8 74 24 ?? 89 54 24", "8B 44 24 ?? 8D 54 24 ?? 52 D8 4C 24",
		"8B 4C 24 ?? 8D 54 24 ?? 52 D8 4C 24");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Singleplayer Camera Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[Camera_SP] - (std::uint8_t*)ExeModule());
			spdlog::info("Multiplayer Camera Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[Camera_MP] - (std::uint8_t*)ExeModule());
			spdlog::info("Car Selection Camera Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CarSelectionAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Fullscreen HUD Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[FullscreenHUD] - (std::uint8_t*)ExeModule());
			spdlog::info("Windowed HUD Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[WindowedHUD] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUD3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[Camera_SP], 8);

			m_singleplayerAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[Camera_SP], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newSingleplayerAspectRatio = s_instance_->m_newAspectRatio;
				*reinterpret_cast<float*>(ctx.esp + 0x10) = s_instance_->m_newSingleplayerAspectRatio;
			});

			Memory::WriteNOPs(AspectRatioScansResult[Camera_MP], 8);

			m_multiplayerAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[Camera_MP], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newMultiplayerAspectRatio = 2.666666746f * s_instance_->m_aspectRatioScale;
				*reinterpret_cast<float*>(ctx.esp + 0x10) = s_instance_->m_newMultiplayerAspectRatio;
			});

			Memory::WriteNOPs(AspectRatioScansResult[CarSelectionAR], 7);

			m_carSelectionAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[CarSelectionAR], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esi + 0x20) = s_instance_->m_newAspectRatio;
			});

			Memory::WriteNOPs(AspectRatioScansResult[FullscreenHUD], 4);

			m_fullscreenHUDAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[FullscreenHUD], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(ctx.esp + 0x3C, ctx.edx);
			});

			Memory::WriteNOPs(AspectRatioScansResult[WindowedHUD], 4);

			m_windowedHUDAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[WindowedHUD], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(ctx.esp + 0x3C, ctx.eax);
			});

			Memory::WriteNOPs(AspectRatioScansResult[HUD3], 4);

			m_HUDAspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[HUD3], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(ctx.esp + 0x3C, ctx.ecx);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 84 30 ?? ?? ?? ?? D8 0D", "D9 03 8B 8E ?? ?? ?? ?? D8 0D", "8B 15 ?? ?? ?? ?? D8 44 24",
		"D8 0D ?? ?? ?? ?? D9 46 ?? D8 4E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Car Selection Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[CarSelectionFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
			spdlog::info("Starting Cutscene Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[StartingCutscene1] - (std::uint8_t*)ExeModule());
			spdlog::info("Starting Cutscene Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[StartingCutscene2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[CarSelectionFOV], 7);

			m_carSelectionFOVHook = safetyhook::create_mid(CameraFOVScansResult[CarSelectionFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.eax + ctx.esi + 0x1F8, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 2);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				float& fCurrentGameplayFOV = *reinterpret_cast<float*>(ctx.ebx);
				s_instance_->m_newGameplayFOV = fCurrentGameplayFOV * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newGameplayFOV);
			});

			m_startingCutsceneAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[StartingCutscene1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[StartingCutscene1], 6);

			m_startingCutsceneFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[StartingCutscene1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_startingCutsceneAddress, EDX, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[StartingCutscene2], 6);

			m_startingCutsceneFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[StartingCutscene2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_startingCutsceneAddress, FMUL, ctx);
			});
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "74 ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());				

				Memory::PatchBytes((uint8_t*)0x0041E104, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideoScanResult = Memory::PatternScan(ExeModule(), "C7 41 ?? ?? ?? ?? ?? C6 41 ?? ?? C6 41 ?? ?? C6 41 ?? ?? C6 41 ?? ?? FF 52 ?? 8B 4C 24 ?? 6A ?? 6A ?? 51",
			"C7 41 44 ?? ?? ?? ?? C6 41 4A 01 C6 41 48 01 C6 41 49 01 C6 41 4B 01 FF 50 20 8B 44 24 40", "C7 41 ?? ?? ?? ?? ?? C6 41 ?? ?? C6 41 ?? ?? C6 41 ?? ?? FF 52",
			"C7 41 44 ?? ?? ?? ?? C6 41 4A 01 C6 41 48 01 C6 41 49 01 C6 41 4B 01 FF 50 20 8B 44 24 50",
			"8B 54 24 ?? 8B 4C 24 ?? 6A ?? 6A ?? 52 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4C 24 ?? 8B 07 51 8B CF FF 50 ?? 8B 54 24 ?? 8B 4C 24 ?? 6A ?? 6A ?? 52 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 44 24");
			if (Memory::AreAllSignaturesValid(SkipIntroVideoScanResult) == true)
			{
				spdlog::info("Black Bea Logo Intro Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[BlackBeaPss] - (std::uint8_t*)ExeModule());
				spdlog::info("Milestone Logo Intro Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[MilestonePss] - (std::uint8_t*)ExeModule());
				spdlog::info("Renderware Logo Intro Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[RenderwarePss] - (std::uint8_t*)ExeModule());
				spdlog::info("Intro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[IntroPss] - (std::uint8_t*)ExeModule());
				spdlog::info("Legal Screen Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[LegalScreen] - (std::uint8_t*)ExeModule());

				Memory::Write(SkipIntroVideoScanResult[BlackBeaPss] + 3, m_newFilename);
				Memory::Write(SkipIntroVideoScanResult[MilestonePss] + 3, m_newFilename);
				Memory::Write(SkipIntroVideoScanResult[RenderwarePss] + 3, m_newFilename);
				Memory::Write(SkipIntroVideoScanResult[IntroPss] + 3, m_newFilename);
				Memory::PatchBytes(SkipIntroVideoScanResult[LegalScreen] + 3, "\x14");
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	const char* m_newFilename = "new.pss";

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_windowedHook{};
	SafetyHookMid m_singleplayerAspectRatioHook{};
	SafetyHookMid m_multiplayerAspectRatioHook{};
	SafetyHookMid m_carSelectionAspectRatioHook{};
	SafetyHookMid m_fullscreenHUDAspectRatioHook{};
	SafetyHookMid m_windowedHUDAspectRatioHook{};
	SafetyHookMid m_HUDAspectRatio3Hook{};
	SafetyHookMid m_carSelectionFOVHook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_startingCutsceneFOV1Hook{};
	SafetyHookMid m_startingCutsceneFOV2Hook{};

	enum ResolutionInstructionsIndices
	{
		Res1,
		Res2,
		Res3
	};

	enum AspectRatioInstructionsIndices
	{
		Camera_SP,
		Camera_MP,
		CarSelectionAR,
		FullscreenHUD,
		WindowedHUD,
		HUD3
	};

	enum CameraFOVInstructionsIndices
	{
		CarSelectionFOV,
		Gameplay,
		StartingCutscene1,
		StartingCutscene2
	};

	enum SkipIntroVideosInstructionsIndex
	{
		BlackBeaPss,
		MilestonePss,
		RenderwarePss,
		IntroPss,
		LegalScreen
	};

	uint8_t m_isWindowedOn = 0;
	float m_newSingleplayerAspectRatio = 0.0f;
	float m_newMultiplayerAspectRatio = 0.0f;
	float m_newHUDAspectRatio = 0.0f;
	uintptr_t m_startingCutsceneAddress = 0;
	float m_currentCameraFOV = 0.0f;
	float m_newGameplayFOV = 0.0f;

	enum InstructionType
	{
		FLD,
		FMUL,
		EDX
	};

	void HUDAspectRatioMidHook(uintptr_t srcAddress, uintptr_t& destAddress)
	{
		float& fCurrentHUDAspectRatio = Memory::ReadMem(srcAddress);
		s_instance_->m_newHUDAspectRatio = fCurrentHUDAspectRatio * s_instance_->m_aspectRatioScale;
		destAddress = std::bit_cast<uintptr_t>(s_instance_->m_newHUDAspectRatio);
	}

	void CameraFOVMidHook(uintptr_t FOVAddress, InstructionType instructionType, SafetyHookContext& ctx)
	{
		m_currentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(m_currentCameraFOV, m_aspectRatioScale);

		switch (instructionType)
		{
			case FLD:
			{
				FPU::FLD(m_newCameraFOV);
				break;
			}

			case FMUL:
			{
				FPU::FMUL(m_newCameraFOV);
				break;
			}

			case EDX:
			{
				ctx.edx = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}
		}
	}

	inline static CorvetteEvolutionGTFix* s_instance_ = nullptr;
};

static std::unique_ptr<CorvetteEvolutionGTFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<CorvetteEvolutionGTFix>(hModule);
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