#include "..\..\common\FixBase.hpp"

class OutlawGolfFix final : public FixBase
{
public:
	explicit OutlawGolfFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~OutlawGolfFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "OutlawGolfWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "Outlaw Golf";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "OLG1PC.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "WindowedWidth", m_newWindowedWidth);
		inipp::get_value(ini.sections["Settings"], "WindowedHeight", m_newWindowedHeight);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_newWindowedWidth);
		spdlog_confparse(m_newWindowedHeight);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "74 ?? 81 F9 ?? ?? ?? ?? 7C", "8B 54 24 ?? 8B 44 24 ?? 89 15",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 33 C0",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F0", "C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 5F 5E 5B", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8D 44 24");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Fullscreen Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[FullscreenListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());
			spdlog::info("Windowed Resolution Instructions 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Windowed1] - (std::uint8_t*)ExeModule());
			spdlog::info("Windowed Resolution Instructions 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Windowed2] - (std::uint8_t*)ExeModule());
			spdlog::info("Windowed Resolution Instructions 3: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Windowed3] - (std::uint8_t*)ExeModule());
			spdlog::info("Windowed Resolution Instructions 4: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Windowed4] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(ResolutionScansResult[FullscreenListUnlock], "\xEB");

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x28);
				s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x2C);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteStaticFOVs();
			});

			Memory::Write(ResolutionScansResult[Windowed1] + 6, m_newWindowedWidth);
			Memory::Write(ResolutionScansResult[Windowed1] + 16, m_newWindowedHeight);
			Memory::Write(ResolutionScansResult[Windowed2] + 6, m_newWindowedWidth);
			Memory::Write(ResolutionScansResult[Windowed2] + 1, m_newWindowedHeight);
			Memory::Write(ResolutionScansResult[Windowed3] + 10, m_newWindowedWidth);
			Memory::Write(ResolutionScansResult[Windowed3] + 3, m_newWindowedHeight);
			Memory::Write(ResolutionScansResult[Windowed4] + 6, m_newWindowedWidth);
			Memory::Write(ResolutionScansResult[Windowed4] + 1, m_newWindowedHeight);
		}

		auto HUDAspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 40 ?? DC C0 D9 1D ?? ?? ?? ?? D9 40 ?? DC C0 D9 1D ?? ?? ?? ?? 8B 48 ?? 81 F9 ?? ?? ?? ?? 74 ?? 39 05 ?? ?? ?? ?? 74 ?? 89 0D ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? A3 ?? ?? ?? ?? 89 15");
		if (HUDAspectRatioScanResult)
		{
			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDAspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(HUDAspectRatioScanResult, 3);

			m_hudAspectRatioHook = safetyhook::create_mid(HUDAspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDAspectRatio = Memory::ReadMem(ctx.eax + 0x68);
				s_instance_->m_newHUDAspectRatio = fCurrentHUDAspectRatio / s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newHUDAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate HUD aspect ratio instruction memory address.");
			return;
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 56",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 35 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 35 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1", "68 ?? ?? ?? ?? F3 ?? 68 ?? ?? ?? ?? 89 2D",
		"68 ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? 83 C4 ?? C6 05", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 74 24",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 68", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 68");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV7] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV8] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV9] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV10] - (std::uint8_t*)ExeModule());
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 50.0f;

	float m_newHUDAspectRatio = 0.0f;

	std::vector<std::uint8_t*> CameraFOVScansResult;

	int m_newWindowedWidth = 0;
	int m_newWindowedHeight = 0;

	enum ResolutionInstructionsIndex
	{
		FullscreenListUnlock,
		WidthHeight,
		Windowed1,
		Windowed2,
		Windowed3,
		Windowed4
	};

	enum AspectRatioInstructionsIndex
	{
		CameraAR1,
		CameraAR2,
		HUDAR
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7,
		FOV8,
		FOV9,
		FOV10
	};

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_hudAspectRatioHook{};

	void WriteStaticFOVs()
	{
		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV, m_aspectRatioScale);

		Memory::Write(CameraFOVScansResult, FOV1, FOV9, 1, m_newCameraFOV * m_fovFactor);
		Memory::Write(CameraFOVScansResult[FOV10] + 1, m_newCameraFOV);
	}

	inline static OutlawGolfFix* s_instance_ = nullptr;
};

static std::unique_ptr<OutlawGolfFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<OutlawGolfFix>(hModule);
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