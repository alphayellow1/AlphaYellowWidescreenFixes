#include "..\..\common\FixBase.hpp"

class YourselfFitnessFix final : public FixBase
{
public:
	explicit YourselfFitnessFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~YourselfFitnessFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Yourself!FitnessFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.4.1";
	}

	const char* TargetName() const override
	{
		return "Yourself!Fitness";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "fitness_pc.exe");
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
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 41 ?? 57 8D 79", "8B 44 24 ?? 8B 54 24 ?? 89 41 ?? 89 51");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());

			m_resolution1Hook = safetyhook::create_mid(ResolutionScansResult[Res1], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0x16E004);
				int& iCurrentHeight = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0x16E008);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_resolution1Hook.disable();
			});

			m_resolution2Hook = safetyhook::create_mid(ResolutionScansResult[Res2], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 8B 54 24 ?? 89 0C");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio2 = 0.75f / s_instance_->m_aspectRatioScale;
				FPU::FMUL(s_instance_->m_newAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "DD 05 ?? ?? ?? ?? 53", "DD 05 ?? ?? ?? ?? D9 F2",
		"DD 05 ?? ?? ?? ?? 8B 0D", "DD 05 ?? ?? ?? ?? A1");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());

			m_cameraFOV1Address = Memory::GetPointerFromAddress(CameraFOVScansResult[FOV1] + 2, Memory::PointerMode::Absolute);
			m_cameraFOV2Address = Memory::GetPointerFromAddress(CameraFOVScansResult[FOV3] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult, FOV1, FOV4, 0, 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraFOV1Address);
			});

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraFOV1Address);
			});

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraFOV2Address);
			});

			m_cameraFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraFOV1Address, s_instance_->m_fovFactor);
			});
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E9 ?? ?? ?? ?? E8",
			"0F 84 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E9 ?? ?? ?? ?? A1", "E8 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? E8",
			"72 ?? 38 1D ?? ?? ?? ?? 0F 84");
			if (Memory::AreAllSignaturesValid(SkipIntroVideosScansResult) == true)
			{
				spdlog::info("Skip Intro Videos Scan: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[RDLogoPlayback] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos Scan: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[YFIntroPlayback] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos Scan: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[DisclaimerDisplay] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos Scan: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[IntroDelayLoop] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScansResult[RDLogoPlayback], "\xE9\xB9\x01\x00\x00\x90");
				Memory::PatchBytes(SkipIntroVideosScansResult[YFIntroPlayback], "\xE9\x45\x01\x00\x00\x90");
				Memory::WriteNOPs(SkipIntroVideosScansResult[DisclaimerDisplay], 5);
				Memory::WriteNOPs(SkipIntroVideosScansResult[IntroDelayLoop], 2);
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_skipIntroVideos = false;

	enum ResolutionInstructionsIndices
	{
		Res1,
		Res2
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4
	};

	enum LogoInstructionsIndices
	{
		RDLogoPlayback,
		YFIntroPlayback,
		DisclaimerDisplay,
		IntroDelayLoop
	};

	SafetyHookMid m_resolution1Hook{};
	SafetyHookMid m_resolution2Hook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};

	double m_fovFactor = 0.0;
	double m_newCameraFOV = 0.0;
	uintptr_t m_cameraFOV1Address;
	uintptr_t m_cameraFOV2Address;	

	void CameraFOVMidHook(uintptr_t FOVAddress, double fovFactor = 1.0)
	{
		double& dCurrentCameraFOV = Memory::ReadMem(FOVAddress);
		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(dCurrentCameraFOV, m_aspectRatioScale, Maths::AngleMode::HalfAngle) * fovFactor;
		FPU::FLD(m_newCameraFOV);
	}

	inline static YourselfFitnessFix* s_instance_ = nullptr;
};

static std::unique_ptr<YourselfFitnessFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<YourselfFitnessFix>(hModule);

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