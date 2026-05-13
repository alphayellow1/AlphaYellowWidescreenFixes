#include "..\..\common\FixBase.hpp"

class BassProShops2007Fix final : public FixBase
{
public:
	explicit BassProShops2007Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BassProShops2007Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BassProShopsTrophyHunter2007WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Bass Pro Shops Trophy Hunter 2007";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "BPS Trophy Hunter 2007.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "C6 44 38 ?? ?? 8B 8E", "8B 44 24 ?? 8B 54 24 ?? 89 81 ?? ?? ?? ?? 8B 44 24 ?? 89 91 ?? ?? ?? ?? 89 81");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::Write(ResolutionScansResult[ResListUnlock] + 4, m_validResolution);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;

				s_instance_->WriteCharacterAndProjectileFOVs();
			});
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 89 85", "68 ?? ?? ?? ?? 51 8B CC 89 64 24 ?? 51", "68 ?? ?? ?? ?? 8B CD E8 ?? ?? ?? ?? 50",
		"C7 44 24 ?? ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D9 44 24 ?? DA E9 DF E0 F6 C4 ?? 7A ?? C7 44 24", "D9 81 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC 56",
		"C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 88 86", "C7 46 ?? ?? ?? ?? ?? 89 48 ?? 8B 15");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("First Person Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FirstPerson1] - (std::uint8_t*)ExeModule());
			spdlog::info("First Person Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FirstPerson2] - (std::uint8_t*)ExeModule());
			spdlog::info("Third Person Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ThirdPerson1] - (std::uint8_t*)ExeModule());
			spdlog::info("Third Person Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ThirdPerson2] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon] - (std::uint8_t*)ExeModule());
			spdlog::info("Projectile Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ProjectileView1] - (std::uint8_t*)ExeModule());
			spdlog::info("Projectile Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ProjectileView2] - (std::uint8_t*)ExeModule());			

			Memory::WriteNOPs(CameraFOVScansResult[Weapon], 6);

			m_weaponFOVHook = safetyhook::create_mid(CameraFOVScansResult[Weapon], [](SafetyHookContext& ctx)
			{
				float& fCurrentWeaponFOV = Memory::ReadMem(ctx.ecx + 0x80);

				s_instance_->m_newWeaponFOV = Maths::CalculateNewFOV_DegBased(fCurrentWeaponFOV, s_instance_->m_aspectRatioScale);

				FPU::FLD(s_instance_->m_newWeaponFOV);
			});			
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_weaponFOVHook{};
	
	static constexpr uint8_t m_validResolution = 1;
	static constexpr float m_originalCharacterFOV1 = 70.0f;
	static constexpr float m_originalCharacterFOV2 = 65.0f;
	static constexpr float m_originalCharacterFOV3 = 60.0f;
	static constexpr float m_originalProjectileViewFOV1 = 90.0f;
	static constexpr float m_originalProjectileViewFOV2 = 95.0f;

	enum ResolutionInstructionsIndex
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum CameraFOVInstructionsIndex
	{
		FirstPerson1,
		FirstPerson2,
		ThirdPerson1,		
		ThirdPerson2,
		Weapon,
		ProjectileView1,
		ProjectileView2
	};

	std::vector<std::uint8_t*> CameraFOVScansResult;

	float m_newCharacterCameraFOV1 = 0.0f;
	float m_newCharacterCameraFOV2 = 0.0f;
	float m_newCharacterCameraFOV3 = 0.0f;
	float m_newWeaponFOV = 0.0f;
	float m_newProjectileCameraFOV1 = 0.0f;
	float m_newProjectileCameraFOV2 = 0.0f;

	void WriteCharacterAndProjectileFOVs()
	{
		m_newCharacterCameraFOV1 = Maths::CalculateNewFOV_DegBased(m_originalCharacterFOV1, m_aspectRatioScale) * m_fovFactor;
		m_newCharacterCameraFOV2 = Maths::CalculateNewFOV_DegBased(m_originalCharacterFOV2, m_aspectRatioScale) * m_fovFactor;
		m_newCharacterCameraFOV3 = Maths::CalculateNewFOV_DegBased(m_originalCharacterFOV3, m_aspectRatioScale) * m_fovFactor;
		Memory::Write(CameraFOVScansResult[FirstPerson1] + 1, m_newCharacterCameraFOV1);
		Memory::Write(CameraFOVScansResult[FirstPerson2] + 1, m_newCharacterCameraFOV1);
		Memory::Write(CameraFOVScansResult[ThirdPerson1] + 1, m_newCharacterCameraFOV3);
		Memory::Write(CameraFOVScansResult[ThirdPerson2] + 4, m_newCharacterCameraFOV2);

		m_newProjectileCameraFOV1 = Maths::CalculateNewFOV_DegBased(m_originalProjectileViewFOV1, m_aspectRatioScale);
		m_newProjectileCameraFOV2 = Maths::CalculateNewFOV_DegBased(m_originalProjectileViewFOV2, m_aspectRatioScale);
		Memory::Write(CameraFOVScansResult[ProjectileView1] + 3, m_newProjectileCameraFOV1);
		Memory::Write(CameraFOVScansResult[ProjectileView2] + 3, m_newProjectileCameraFOV2);
	}

	inline static BassProShops2007Fix* s_instance_ = nullptr;
};

static std::unique_ptr<BassProShops2007Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BassProShops2007Fix>(hModule);
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