#include "..\..\common\FixBase.hpp"

class UrbanFreestyleSoccerFix final : public FixBase
{
public:
	explicit UrbanFreestyleSoccerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~UrbanFreestyleSoccerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "UrbanFreestyleSoccerWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Urban Freestyle Soccer";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "PlayUFS.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);		

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 0F 89 4E ?? 8B 57 ?? 89 56 ?? 8B 47 ?? 89 86", "8B 08 89 54 24 ?? 8B 50 ?? 8B 40 ?? 89 44 24");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[Res1], 2);

			m_resolutionWidth1Hook = safetyhook::create_mid(ResolutionScansResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionScansResult[Res1] + 5, 3);

			m_resolutionHeight1Hook = safetyhook::create_mid(ResolutionScansResult[Res1] + 5, [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::PatchBytes(ResolutionScansResult[Res2], 2);

			m_resolutionWidth2Hook = safetyhook::create_mid(ResolutionScansResult[Res2], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionScansResult[Res2] + 48, 3);

			m_resolutionHeight2Hook = safetyhook::create_mid(ResolutionScansResult[Res2] + 48, [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C7 86 ?? ?? ?? ?? ?? ?? ?? ?? 8B 83",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? A0");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScansResult[AR1] + 6, m_newAspectRatio);
			Memory::Write(AspectRatioScansResult[AR1] + 18, m_newAspectRatio);
			Memory::Write(AspectRatioScansResult[AR2] + 6, m_newAspectRatio);
			Memory::Write(AspectRatioScansResult[AR2] + 18, m_newAspectRatio);
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "DB 87 ?? ?? ?? ?? D8 0D", "DB 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DC 0D", "DB 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DC 0D");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edi + 0xC0, FILD, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0xC0, FILD, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV3], 6);

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ecx + 0xC0, EDX, ctx);
			});
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? 50 FF 15 ?? ?? ?? ?? C7 05");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideoScanResult = Memory::PatternScan(ExeModule(), "E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 32 DB", "72 ?? E8 ?? ?? ?? ?? E8", "0F B6 44 24 ?? 83 E8",
			"E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 6A ?? 6A ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 5F");
			if (Memory::AreAllSignaturesValid(SkipIntroVideoScanResult) == true)
			{
				spdlog::info("Acclaim Intro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[AcclaimVid] - (std::uint8_t*)ExeModule());
				spdlog::info("Legal Screen Timer Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[LegalScreenTimerCheck] - (std::uint8_t*)ExeModule());
				spdlog::info("Gusto Games Image Fade/Loop Logic Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[GustoGamesLoop] - (std::uint8_t*)ExeModule());
				spdlog::info("UFIntro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[UFIntro] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(SkipIntroVideoScanResult[AcclaimVid], 5);
				Memory::WriteNOPs(SkipIntroVideoScanResult[LegalScreenTimerCheck], 2);
				Memory::PatchBytes(SkipIntroVideoScanResult[GustoGamesLoop], "\xE9\xAC\x00\x00\x00");
				Memory::WriteNOPs(SkipIntroVideoScanResult[UFIntro], 5);
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	uint32_t m_newCameraFOV = 0;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_resolutionWidth1Hook{};
	SafetyHookMid m_resolutionHeight1Hook{};
	SafetyHookMid m_resolutionWidth2Hook{};
	SafetyHookMid m_resolutionHeight2Hook{};

	enum ResolutionInstructionsIndex
	{
		Res1,
		Res2
	};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2,
		FOV3
	};

	enum SkipIntroVideosInstructionsIndex
	{
		AcclaimVid,
		LegalScreenTimerCheck,
		GustoGamesLoop,
		UFIntro
	};

	enum DestInstruction
	{
		FILD,
		EDX
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, DestInstruction destInstruction, SafetyHookContext& ctx)
	{
		int& iCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = (int)(iCurrentCameraFOV * m_fovFactor);

		switch (destInstruction)
		{
			case FILD:
			{
				FPU::FILD(m_newCameraFOV);
				break;
			}

			case EDX:
			{
				ctx.edx = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}
		}
	}

	inline static UrbanFreestyleSoccerFix* s_instance_ = nullptr;
};

static std::unique_ptr<UrbanFreestyleSoccerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<UrbanFreestyleSoccerFix>(hModule);
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