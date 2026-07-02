#include "..\..\common\FixBase.hpp"

class MXVsATVUnleashedFix final : public FixBase
{
public:
	explicit MXVsATVUnleashedFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MXVsATVUnleashedFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MXvsATVUnleashedWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "MX vs. ATV Unleashed";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "MXvsATV.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "Framerate", m_newFramerate);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_newFramerate);
	}

	void ApplyFix() override
	{
		auto ResolutionListScanResult = Memory::PatternScan(ExeModule(), "C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C3 8B 44 24");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListScanResult - (std::uint8_t*)ExeModule());

			// 640x480
			Memory::Write(ResolutionListScanResult + 2, m_newResX);
			Memory::Write(ResolutionListScanResult + 8, m_newResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 33, m_newResX);
			Memory::Write(ResolutionListScanResult + 39, m_newResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 64, m_newResX);
			Memory::Write(ResolutionListScanResult + 70, m_newResY);

			// 1152x864
			Memory::Write(ResolutionListScanResult + 95, m_newResX);
			Memory::Write(ResolutionListScanResult + 101, m_newResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 126, m_newResX);
			Memory::Write(ResolutionListScanResult + 132, m_newResY);

			// 1600x1200
			Memory::Write(ResolutionListScanResult + 157, m_newResX);
			Memory::Write(ResolutionListScanResult + 163, m_newResY);
		}
		else
		{
			spdlog::info("Failed to locate the resolution list scan memory address.");
			return;
		}

		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D8 F1 D9 1D ?? ?? ?? ?? D9 C9");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScanResult + 2, &m_newAspectRatio);
			Memory::Write(AspectRatioScanResult + 18, &m_newAspectRatio);
		}
		else
		{
			spdlog::info("Failed to locate the aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? 51 D8 0D ?? ?? ?? ?? D9 1C", "8D 54 24 ?? 52 6A ?? 6A ?? 6A ?? 8D 84 24",
		"57 6A ?? 6A ?? 6A ?? 8D 8E", "8B 0C 07 89 4E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Chase Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ChaseCam] - (std::uint8_t*)ExeModule());
			spdlog::info("Cinematic Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[CinematicCam] - (std::uint8_t*)ExeModule());
			spdlog::info("Helmet Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[HelmetCam] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 4);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGeneralFOV = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentGeneralFOV, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newCameraFOV);
			});

			m_chaseCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[ChaseCam], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esp + 0x28) = *reinterpret_cast<float*>(ctx.esp + 0x28) * s_instance_->m_fovFactor;
			});

			m_cinematicCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[CinematicCam], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.edi) = *reinterpret_cast<float*>(ctx.edi) * s_instance_->m_fovFactor;
			});

			Memory::WriteNOPs(CameraFOVScansResult[HelmetCam], 3);

			m_helmetCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[HelmetCam], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHelmetFOV = Memory::ReadMem(ctx.edi + ctx.eax);
				s_instance_->m_newHelmetFOV = s_instance_->m_currentHelmetFOV * s_instance_->m_fovFactor;
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newHelmetFOV);
			});
		}

		auto FramerateScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? B9");
		if (FramerateScanResult)
		{
			spdlog::info("Framerate Instruction: Address is{:s} + {:x}", ExeName().c_str(), FramerateScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(FramerateScanResult + 1, m_newFramerate);
		}
		else
		{
			spdlog::info("Cannot locate the framerate instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newFramerate = 0.0f;

	float m_currentGeneralFOV = 0.0f;
	float m_newGeneralFOV = 0.0f;
	float m_currentHelmetFOV = 0.0f;
	float m_newHelmetFOV = 0.0f;

	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_chaseCameraFOVHook{};
	SafetyHookMid m_cinematicCameraFOVHook{};
	SafetyHookMid m_helmetCameraFOVHook{};

	enum ResolutionListScansIndex
	{
		List1,
		ResStrings
	};

	enum CameraFOVInstructionsIndex
	{
		General,
		ChaseCam,
		CinematicCam,
		HelmetCam
	};

	inline static MXVsATVUnleashedFix* s_instance_ = nullptr;
};

static std::unique_ptr<MXVsATVUnleashedFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<MXVsATVUnleashedFix>(hModule);
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