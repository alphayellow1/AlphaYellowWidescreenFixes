#include "..\..\common\FixBase.hpp"

class BarbieRidingCampFix final : public FixBase
{
public:
	explicit BarbieRidingCampFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BarbieRidingCampFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BarbieHorseAdventuresRidingCampWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3.1";
	}

	const char* TargetName() const override
	{
		return "Barbie Horse Adventures: Riding Camp";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "PXGameStudioRuntime2008.exe");
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

		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "B8 ?? ?? ?? ?? 89 3D");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			// 800x600 fallback (lower res)
			Memory::Write(ResolutionScanResult + 1, m_newResX);
			Memory::Write(ResolutionScanResult + 26, m_newResY);

			// 1024x768 fallback (higher res)
			Memory::Write(ResolutionScanResult + 45, m_newResX);
			Memory::Write(ResolutionScanResult + 50, m_newResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 40 ?? D8 4C 24 ?? D9 5C 24 ?? D9 44 24 ?? D9 5C 24 ?? D9 40 ?? D9 1C 24 50 E8 ?? ?? ?? ?? 83 C4 ?? 5E");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 3);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVInstructionScanResult = Memory::PatternScan(ExeModule(), "D9 47 ?? D9 58 ?? D9 44 24");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 3);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.edi + 0x7C);
				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "74 ?? 8D 8C 24 ?? ?? ?? ?? E8");
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
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "E8 ?? ?? ?? ?? 8B 16 8B 42 ?? 53");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(SkipIntroVideosScanResult, 5);
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	inline static BarbieRidingCampFix* s_instance_ = nullptr;
};

static std::unique_ptr<BarbieRidingCampFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<BarbieRidingCampFix>(hModule);
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