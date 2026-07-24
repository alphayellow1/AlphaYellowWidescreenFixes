#include "..\..\common\FixBase.hpp"

class DreamPinball3DFix final : public FixBase
{
public:
	explicit DreamPinball3DFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DreamPinball3DFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DreamPinball3DWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Dream Pinball 3D";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "dp3d.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroLogos", m_skipIntroLogos);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroLogos);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8",
		"68 ?? ?? ?? ?? 83 F2", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 15 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 75");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Main Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[MainMenu1] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[MainMenu2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResList] - (std::uint8_t*)ExeModule());

			Memory::Write(ResolutionScansResult[MainMenu1] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[MainMenu1] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[MainMenu1] + 16, m_newResX);
			Memory::Write(ResolutionScansResult[MainMenu1] + 26, m_newResY);

			Memory::Write(ResolutionScansResult[MainMenu2] + 12, m_newResX);
			Memory::Write(ResolutionScansResult[MainMenu2] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[MainMenu2] + 22, m_newResX);
			Memory::Write(ResolutionScansResult[MainMenu2] + 17, m_newResY);
			Memory::Write(ResolutionScansResult[MainMenu2] + 32, m_newResX);
			Memory::Write(ResolutionScansResult[MainMenu2] + 42, m_newResY);

			Memory::Write(ResolutionScansResult[ResList] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 16, m_newResY);
			Memory::Write(ResolutionScansResult[ResList] + 40, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 50, m_newResY);
			Memory::Write(ResolutionScansResult[ResList] + 78, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 88, m_newResY);
			Memory::Write(ResolutionScansResult[ResList] + 116, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 126, m_newResY);
			Memory::Write(ResolutionScansResult[ResList] + 154, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 164, m_newResY);
			Memory::Write(ResolutionScansResult[ResList] + 192, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 202, m_newResY);
			Memory::Write(ResolutionScansResult[ResList] + 230, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 240, m_newResY);
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "74 ?? D9 05 ?? ?? ?? ?? D9 1D");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(AspectRatioScanResult, "\xEB");
		}
		else
		{
			spdlog::info("Failed to locate the aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D9 15 ?? ?? ?? ?? 8B 46");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_cameraFOVAddress = Memory::GetPointerFromAddress(CameraFOVScanResult + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(s_instance_->m_cameraFOVAddress);
				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::info("Failed to locate the camera FOV instruction memory address.");
			return;
		}

		if (m_skipIntroLogos == true)
		{
			auto SkipIntroLogosScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 8D 44 24 ?? 68 ?? ?? ?? ?? 50 89 2D");
			if (SkipIntroLogosScanResult)
			{
				spdlog::info("Skip Intro Logos Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroLogosScanResult, "\xE9\xCE\x02\x00\x00");
			}
			else
			{
				spdlog::error("Failed to locate skip intro logos instruction memory address.");
				return;
			}
		}
	}

private:
	bool m_skipIntroLogos = false;

	uintptr_t m_cameraFOVAddress = 0;

	SafetyHookMid m_cameraFOVHook{};

	enum ResolutionInstructionsIndices
	{
		MainMenu1,
		MainMenu2,
		ResList
	};

	inline static DreamPinball3DFix* s_instance_ = nullptr;
};

static std::unique_ptr<DreamPinball3DFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<DreamPinball3DFix>(hModule);
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