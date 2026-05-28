#include "..\..\common\FixBase.hpp"

class BackyardSkateboardingFix final : public FixBase
{
public:
	explicit BackyardSkateboardingFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BackyardSkateboardingFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BackyardSkateboardingWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Backyard Skateboarding";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "BYSkateboarding.exe") ||
		Util::stringcmp_caseless(exeName, "BYSkateboarding_DX8.1.exe") || 
		Util::stringcmp_caseless(exeName, "BYSkateboarding_DX8.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);		
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		ResolutionScan1Result = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 74 ?? C7 44 24");
		ResolutionScan2Result = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 74 ?? 8B 08 50 FF 51 ?? 3B C7");

		if (ResolutionScan1Result != nullptr)
		{
			m_gameVersion = BS_ORIGINAL;
			ResolutionScanResult = ResolutionScan1Result;
		}
		else if (ResolutionScan2Result != nullptr)
		{
			m_gameVersion = BS_GOTY;
			ResolutionScanResult = ResolutionScan2Result;
		}

		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			if (m_gameVersion == BS_ORIGINAL)
			{
				// 640x480
				Memory::Write(ResolutionScanResult + 4, m_newResX);

				Memory::Write(ResolutionScanResult + 12, m_newResY);
			}
			else if (m_gameVersion == BS_GOTY)
			{
				// 640x480
				Memory::Write(ResolutionScanResult + 4, m_newResX);
				Memory::Write(ResolutionScanResult + 12, m_newResX);

				// 800x600
				Memory::Write(ResolutionScanResult + 55, m_newResX);
				Memory::Write(ResolutionScanResult + 63, m_newResY);
			}
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		if (m_gameVersion == BS_ORIGINAL)
		{
			CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 46 ?? 83 C4 ?? 68");
		}
		else if (m_gameVersion == BS_GOTY)
		{
			CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 4E ?? 83 C4 ?? 50 51");
		}

		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 3);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.esi + 0x2C);
				s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;

				switch (s_instance_->m_gameVersion)
				{
					case BS_ORIGINAL:
					{
						ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
						break;
					}

					case BS_GOTY:
					{
						ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
						break;
					}
				}
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		auto HUDHorizontalResScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 8B 4C 24 ?? 89 04 ?? 8B 44 24 ?? 8D 14");
		if (HUDHorizontalResScanResult)
		{
			spdlog::info("HUD Horizontal Res Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDHorizontalResScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(HUDHorizontalResScanResult, 4);

			m_hudHorizontalResHook = safetyhook::create_mid(HUDHorizontalResScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDVerticalRes = Memory::ReadMem(ctx.esp + 0x14);
				s_instance_->m_newHUDHorizontalRes = fCurrentHUDVerticalRes * s_instance_->m_newAspectRatio;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newHUDHorizontalRes);
			});
		}
		else
		{
			spdlog::error("Failed to locate HUD horizontal resolution instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_cameraFOVHook{};
	SafetyHookMid m_hudHorizontalResHook{};

	uint8_t* ResolutionScanResult = nullptr;
	uint8_t* ResolutionScan1Result = nullptr;
	uint8_t* ResolutionScan2Result = nullptr;
	uint8_t* CameraFOVScanResult = nullptr;

	float m_newHUDHorizontalRes = 0.0f;
	int m_gameVersion = 0;

	enum GameVersions
	{
		BS_ORIGINAL,
		BS_GOTY
	};

	inline static BackyardSkateboardingFix* s_instance_ = nullptr;
};

static std::unique_ptr<BackyardSkateboardingFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BackyardSkateboardingFix>(hModule);
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