#include "..\..\common\FixBase.hpp"

class RainbowSixLockdownFix final : public FixBase
{
public:
	explicit RainbowSixLockdownFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RainbowSixLockdownFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TomClancysRainbowSixLockdownWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Tom Clancy's Rainbow Six: Lockdown";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Lockdown.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "MainMenuWidth", m_newMainMenuResX);
		inipp::get_value(ini.sections["Settings"], "MainMenuHeight", m_newMainMenuResY);
		inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", m_cameraFOVFactor);
		inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", m_weaponFOVFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		
		FallbackToDesktopResolution(m_newMainMenuResX, m_newMainMenuResY);
		
		spdlog_confparse(m_newMainMenuResX);
		spdlog_confparse(m_newMainMenuResY);;
		spdlog_confparse(m_cameraFOVFactor);
		spdlog_confparse(m_weaponFOVFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 92 ?? ?? ?? ?? 8B 06",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 92 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? E8", "8B 5C 24 ?? 8B 6C 24 ?? 74");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Main Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[MainMenu1] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[MainMenu2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			// Main Menu is 1024x768
			Memory::Write(m_resolutionScansResult[MainMenu1] + 6, m_newMainMenuResX);
			Memory::Write(m_resolutionScansResult[MainMenu1] + 1, m_newMainMenuResY);
			Memory::Write(m_resolutionScansResult[MainMenu2] + 6, m_newMainMenuResX);
			Memory::Write(m_resolutionScansResult[MainMenu2] + 1, m_newMainMenuResY);

			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x24);
				s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x28);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 54 24 ?? 89 91 ?? ?? ?? ?? A1 ?? ?? ?? ?? D9 80", "DD 05 ?? ?? ?? ?? 8B 96");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[Camera] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[Weapon] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_cameraFOVScansResult[Camera], 4);

			m_cameraFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Camera], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCameraFOV = Memory::ReadMem(ctx.esp + 0x4);

				if (s_instance_->m_currentCameraFOV == fDefaultCameraFOV)
				{
					s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_cameraFOVFactor;
				}
				else
				{
					s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentCameraFOV, s_instance_->m_aspectRatioScale);
				}

				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
			});

			m_weaponFOVAddress = Memory::GetPointerFromAddress(m_cameraFOVScansResult[Weapon] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(m_cameraFOVScansResult[Weapon], 6);

			m_weaponFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[Weapon], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWeaponFOV = Memory::ReadMem(s_instance_->m_weaponFOVAddress);
				s_instance_->m_newWeaponFOV = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentWeaponFOV, s_instance_->m_aspectRatioScale, Maths::AngleMode::HalfAngle) * s_instance_->m_weaponFOVFactor;
				FPU::FLD(s_instance_->m_newWeaponFOV);
			});
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "74 ?? 8B 0D ?? ?? ?? ?? 8B 01 FF 50",
			"74 ?? 8B 15 ?? ?? ?? ?? 8B 82 ?? ?? ?? ?? 8B 4C 24 ?? 8B 49", "74 ?? E8 ?? ?? ?? ?? 8B 84 24 ?? ?? ?? ?? 53",
			"85 FF 75 ?? 85 ED B8", "43 E9 ?? ?? ?? ?? 8B 87");
			if (Memory::AreAllSignaturesValid(m_skipIntroVideosScansResult) == true)
			{
				spdlog::info("Ubisoft Logo Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScansResult[UbisoftLogo] - (std::uint8_t*)ExeModule());
				spdlog::info("Red Storm Logo Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScansResult[RedStormLogo] - (std::uint8_t*)ExeModule());
				spdlog::info("Game Logo Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScansResult[GameLogo] - (std::uint8_t*)ExeModule());
				spdlog::info("Display coordinate check: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScansResult[CoordinateCheck] - (std::uint8_t*)ExeModule());
				spdlog::info("Next display path: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScansResult[NextDisplay] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_skipIntroVideosScansResult[UbisoftLogo], "\xEB");
				Memory::PatchBytes(m_skipIntroVideosScansResult[RedStormLogo], "\xEB");
				Memory::PatchBytes(m_skipIntroVideosScansResult[GameLogo], "\xEB");

				m_primaryWindowOnlyHook = safetyhook::create_mid(m_skipIntroVideosScansResult[CoordinateCheck], [](SafetyHookContext & ctx)
				{
					if (ctx.edi != 0 || ctx.ebp != 0)
					{
						ctx.eip = (uintptr_t)s_instance_->m_skipIntroVideosScansResult[NextDisplay];
					}
				});
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float fDefaultCameraFOV = 1.3962634801864624f;

	int m_newMainMenuResX = 0;
	int m_newMainMenuResY = 0;

	float m_currentCameraFOV = 0.0f;	
	uintptr_t m_weaponFOVAddress = 0;
	double m_currentWeaponFOV = 0.0;
	double m_newWeaponFOV = 0.0;
	float m_cameraFOVFactor = 0.0f;
	double m_weaponFOVFactor = 0.0;

	bool m_skipIntroVideos = false;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::vector<std::uint8_t*> m_skipIntroVideosScansResult{};

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};
	SafetyHookMid m_weaponFOVHook{};
	SafetyHookMid m_primaryWindowOnlyHook{};

	enum ResolutionsInstructionsIndices
	{
		MainMenu1,
		MainMenu2,
		WidthHeight
	};

	enum CameraFOVInstructionsIndices
	{
		Camera,
		Weapon
	};

	enum SkipIntroVideosInstructionsIndices
	{
		UbisoftLogo,
		RedStormLogo,
		GameLogo,
		CoordinateCheck,
		NextDisplay
	};

	inline static RainbowSixLockdownFix* s_instance_ = nullptr;
};

static std::unique_ptr<RainbowSixLockdownFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<RainbowSixLockdownFix>(hModule);
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