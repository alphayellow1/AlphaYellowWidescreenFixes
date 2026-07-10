#include "..\..\common\FixBase.hpp"

class AgassiFix final : public FixBase
{
public:
	explicit AgassiFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AgassiFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AgassiTennisGenerationFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3.1";
	}

	const char* TargetName() const override
	{
		return "Agassi Tennis Generation";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AGASSI.exe");
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
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "85 F6 0F 84 ?? ?? ?? ?? 8B 4C 24 ?? 81 F9", "8B 32 89 35 ?? ?? ?? ?? 8B 72");
		
		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 B4 81 ?? ?? ?? ?? D9 5C 24 ?? 8B 44 81 ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? C2 ?? ?? 90 90 90 90 90 90 90 90 83 EC",
		"D8 B4 86 ?? ?? ?? ?? D9 5C 24 ?? 8B 54 86 ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 5F 5E 5B 83 C4 ?? C2 ?? ?? 83 FF ?? 0F 85", "D8 B4 85 ?? ?? ?? ?? D9 5C 24");

		auto CameraFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "89 94 81 ?? ?? ?? ?? 75",
		"89 94 8E ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? D9 84 86 ?? ?? ?? ?? 8D 4C 24 ?? D8 8C 86 ?? ?? ?? ?? 51 D9 5C 24 ?? D9 84 86 ?? ?? ?? ?? D8 8C 86 ?? ?? ?? ?? D8 B4 86 ?? ?? ?? ?? D9 5C 24 ?? 8B 54 86 ?? 52 E8 ?? ?? ?? ?? 83 C4 ?? 5F 5E 5B 83 C4 ?? C2 ?? ?? 83 FF ?? 0F 85",
		"89 8C 85 ?? ?? ?? ?? 75");

		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 66);

			Memory::PatchBytes(ResolutionScansResult[ResListUnlock], "\x8B\x4C\x24\x18\x8B\x44\x24\x1C\x8B\x54\x24\x20");

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Menu Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[MenuAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CutscenesAR] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult, MenuAR, CutscenesAR, 0, 7);

			m_menuAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[MenuAR], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioMidHook(ctx);
			});

			m_gameplayAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[GameplayAR], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioMidHook(ctx);
			});

			m_cutscenesAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[CutscenesAR], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioMidHook(ctx);
			});
		}
		
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Menu Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[MenuFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[GameplayFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[CutscenesFOV] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionsScansResult, MenuFOV, CutscenesFOV, 0, 7);

			m_menuFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MenuFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edx, ctx.ecx + ctx.eax * 0x4 + 0xA4);
			});

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GameplayFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edx, ctx.esi + ctx.ecx * 0x4 + 0xA4, s_instance_->m_fovFactor);
			});

			m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[CutscenesFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ecx, ctx.ebp + ctx.eax * 0x4 + 0xA4);
			});
		}

		if (m_skipIntroVideos == true)
		{
			auto IntroVideosPlaybackScansResult = Memory::PatternScan(ExeModule(), "E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D ?? ?? ?? ?? 8D 44 24 ?? 50 68 ?? ?? ?? ?? 56 51 E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 15 ?? ?? ?? ?? 56 52 FF 15 ?? ?? ?? ?? 68",
			"E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 15 ?? ?? ?? ?? 56 52 FF 15 ?? ?? ?? ?? 68",
			"E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D ?? ?? ?? ?? 8D 44 24 ?? 50 68 ?? ?? ?? ?? 56 51 E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 15 ?? ?? ?? ?? 56 52 FF 15 ?? ?? ?? ?? 5E",
			"E8 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 15 ?? ?? ?? ?? 56 52 FF 15 ?? ?? ?? ?? 5E");
			if (Memory::AreAllSignaturesValid(IntroVideosPlaybackScansResult) == true)
			{
				spdlog::info("Intro Videos Playback Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), IntroVideosPlaybackScansResult[IntroVid1] - (std::uint8_t*)ExeModule());
				spdlog::info("Intro Videos Playback Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), IntroVideosPlaybackScansResult[IntroVid2] - (std::uint8_t*)ExeModule());
				spdlog::info("Intro Videos Playback Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), IntroVideosPlaybackScansResult[IntroVid3] - (std::uint8_t*)ExeModule());
				spdlog::info("Intro Videos Playback Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), IntroVideosPlaybackScansResult[IntroVid4] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(IntroVideosPlaybackScansResult, IntroVid1, IntroVid4, 0, "\xB8\x01\x00\x00\x00");
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_skipIntroVideos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_menuAspectRatioHook{};
	SafetyHookMid m_gameplayAspectRatioHook{};
	SafetyHookMid m_cutscenesAspectRatioHook{};
	SafetyHookMid m_menuFOVHook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_cutscenesFOVHook{};

	enum ResolutionInstructionsIndex
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum AspectRatioInstructionsIndex
	{
		MenuAR,
		GameplayAR,
		CutscenesAR
	};

	enum CameraFOVInstructionsIndex
	{
		MenuFOV,
		GameplayFOV,
		CutscenesFOV
	};

	enum IntroVideosPlaybackIntructionsIndex
	{
		IntroVid1,
		IntroVid2,
		IntroVid3,
		IntroVid4
	};

	void AspectRatioMidHook(SafetyHookContext& ctx)
	{
		FPU::FDIV(m_newAspectRatio);
	}

	void CameraFOVMidHook(uintptr_t SourceAddress, uintptr_t DestinationAddress, float fovFactor = 1.0f)
	{
		const float& fCurrentCameraFOV = Memory::ReadRegister(SourceAddress);

		m_newCameraFOV = fCurrentCameraFOV * m_aspectRatioScale * fovFactor;

		*reinterpret_cast<float*>(DestinationAddress) = m_newCameraFOV;
	}

	inline static AgassiFix* s_instance_ = nullptr;
};

static std::unique_ptr<AgassiFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AgassiFix>(hModule);
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