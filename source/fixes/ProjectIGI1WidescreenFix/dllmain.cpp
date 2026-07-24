#include "..\..\common\FixBase.hpp"

class ProjectIGI1Fix final : public FixBase
{
public:
	explicit ProjectIGI1Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ProjectIGI1Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ProjectIGI1WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.7";
	}

	const char* TargetName() const override
	{
		return "Project IGI 1: I'm Going In";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "IGI.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "CameraFOVFactor", m_cameraFOVFactor);
		inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", m_weaponFOVFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroLogos", m_skipIntroLogos);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_cameraFOVFactor);
		spdlog_confparse(m_weaponFOVFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroLogos);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 45 ?? 89 44 24 ?? 8B 4D");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ebp + 0xC) = s_instance_->m_newResX;
				*reinterpret_cast<int*>(ctx.ebp + 0x10) = s_instance_->m_newResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? DB 44 24 ?? D8 0D");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			m_newAspectRatio2 = m_originalAspectRatio * m_aspectRatioScale;

			Memory::Write(AspectRatioScanResult + 2, &m_newAspectRatio2);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(),
		/*General*/
		"D9 40 ?? D8 4B ?? D9 1C 24",
		/*Hipfire*/
		"DD 05 ?? ?? ?? ?? D9 F2 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8D 54 24", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D8 9D",
		"DD 05 ?? ?? ?? ?? D9 F2 83 C4 ?? 89 9E", "DD 05 ?? ?? ?? ?? D9 F2 53", "DD 05 ?? ?? ?? ?? D9 F2 8B 44 24 ?? C7 80", "DD 05 ?? ?? ?? ?? D9 F2 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8D 44 24",
		"DD 05 ?? ?? ?? ?? D9 F2 8B 4C 24", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 5C 24 ?? D8 54 24", "DD 05 ?? ?? ?? ?? D9 F2 8B 44 24 ?? DD D8", "DD 05 ?? ?? ?? ?? D9 F2 8B 44 24 ?? 33 C9", "DD 05 ?? ?? ?? ?? D9 F2 83 C4 ?? DD D8",
		"DD 05 ?? ?? ?? ?? D9 F2 B9 ?? ?? ?? ?? 8D B4 24 ?? ?? ?? ?? 8D BC 24 ?? ?? ?? ?? 83 C4 ?? 89 AC 24", "DD 05 ?? ?? ?? ?? D9 F2 B9 ?? ?? ?? ?? 8D B4 24 ?? ?? ?? ?? 8D BC 24 ?? ?? ?? ?? 83 C4 ?? F3 A5",
		/*Weapon*/
		"DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 9E", "DD 05 ?? ?? ?? ?? D9 F2 8B 54 24", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 98 ?? ?? ?? ?? C3", "DD 05 ?? ?? ?? ?? D9 F2 5F", "DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 5C 24 ?? 8B 44 24",
		"DD 05 ?? ?? ?? ?? D9 F2 DD D8 D9 98 ?? ?? ?? ?? 89 97");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire1] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire2] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire3] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire4] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire5] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire6] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire7] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire8] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire9] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 10: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire10] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 11: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire11] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 12: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire12] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 13: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire13] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon1] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon2] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon3] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon4] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon5] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon6] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 3);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = Memory::ReadMem(ctx.eax + 0x40);

				if (s_instance_->m_insideComputer == 0)
				{
					s_instance_->m_newGeneralFOV = fCurrentGeneralFOV * s_instance_->m_aspectRatioScale;
				}
				else if (s_instance_->m_insideComputer == 1)
				{
					s_instance_->m_newGeneralFOV = (fCurrentGeneralFOV * s_instance_->m_aspectRatioScale) / (float)s_instance_->m_cameraFOVFactor;
				}

				FPU::FLD(s_instance_->m_newGeneralFOV);
			});

			m_newHipfireFOV = m_originalHipfireFOV * m_cameraFOVFactor;

			Memory::Write(CameraFOVScansResult, Hipfire1, Hipfire13, 2, &m_newHipfireFOV);

			m_newWeaponFOV = Maths::CalculateNewFOV_RadBased(m_originalWeaponFOV, m_aspectRatioScale, Maths::AngleMode::HalfAngle) * m_weaponFOVFactor;

			Memory::Write(CameraFOVScansResult, Weapon1, Weapon6, 2, &m_newWeaponFOV);
		}

		auto InsideComputerScanResult = Memory::PatternScan(ExeModule(), "A2 ?? ?? ?? ?? D9 5C 24 10 F3 A5 D9 E0 D9 44 24 0C D9 E0 D9 44 24 10 D9 E0 D9 44 24 1C D8 C9 D9 44 24 18 D8 CB DE C1 D9 44 24 14 D8 CC DE C1 D9 5C 24 08 D9 44 24 28 D8 C9 D9 44 24 24 D8 CB DE C1 D9 44 24 20 D8 CC DE C1 D9 5C 24 0C D9 44 24 34 D8 C9 D9 44 24 30 D8 CB DE C1 D9 44 24 2C D8 CC DE C1 D9 5C 24 10 8B 44 24 08 B9 0A 00 00 00 8D 74 24 14");
		if (InsideComputerScanResult)
		{
			spdlog::info("Inside Computer Instruction: Address is {:s}+{:x}", ExeName().c_str(), InsideComputerScanResult - (std::uint8_t*)ExeModule());

			m_insideComputerHook = safetyhook::create_mid(InsideComputerScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_insideComputer = static_cast<uint8_t>(ctx.eax & 0xFF);
			});
		}
		else
		{
			spdlog::error("Failed to locate inside computer instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? FF D6");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Checks Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(RunMultipleInstancesCheckScanResult, 6);
				Memory::WriteNOPs(RunMultipleInstancesCheckScanResult + 19, 6);
				Memory::WriteNOPs(RunMultipleInstancesCheckScanResult + 30, 6);
			}
			else
			{
				spdlog::error("Failed to locate multiple instance checks instructions scan memory address.");
				return;
			}
		}

		if (m_skipIntroLogos == true)
		{
			auto SkipIntroLogosScanResult = Memory::PatternScan(ExeModule(), "6A ?? E8 ?? ?? ?? ?? 83 C4 ?? B0 ?? C3 90");
			if (SkipIntroLogosScanResult)
			{
				spdlog::info("Skip Intro Logos Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroLogosScanResult + 1, "\x03");
			}
			else
			{
				spdlog::error("Failed to locate skip intro logos instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalAspectRatio = 4.0f;
	static constexpr double m_originalHipfireFOV = 0.785398185253143;
	static constexpr double m_originalWeaponFOV = 0.523598790168762;

	bool m_runMultipleInstances = false;
	bool m_skipIntroLogos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_insideComputerHook{};
	SafetyHookMid m_briefingMapFOVHook{};

	float m_newAspectRatio2 = 0.0f;
	float m_newGeneralFOV = 0.0f;
	double m_newHipfireFOV = 0.0;
	double m_newWeaponFOV = 0.0;
	uint8_t m_insideComputer = 0;

	double m_cameraFOVFactor = 0.0;
	double m_weaponFOVFactor = 0.0;	

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2,
		AR3,
		AR4,
		AR5,
		AR6,
		AR7,
		AR8,
		AR9,
		AR10,
		AR11,
		AR12,
		AR13,
		AR14
	};

	enum CameraFOVInstructionsIndices
	{
		General,
		Hipfire1,
		Hipfire2,
		Hipfire3,
		Hipfire4,
		Hipfire5,
		Hipfire6,
		Hipfire7,
		Hipfire8,
		Hipfire9,
		Hipfire10,
		Hipfire11,
		Hipfire12,
		Hipfire13,
		Weapon1,
		Weapon2,
		Weapon3,
		Weapon4,
		Weapon5,
		Weapon6
	};

	inline static ProjectIGI1Fix* s_instance_ = nullptr;
};

static std::unique_ptr<ProjectIGI1Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<ProjectIGI1Fix>(hModule);
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