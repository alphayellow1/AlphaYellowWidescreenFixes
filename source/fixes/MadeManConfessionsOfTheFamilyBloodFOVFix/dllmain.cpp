#include "..\..\common\FixBase.hpp"

class MadeManFix final : public FixBase
{
public:
	explicit MadeManFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MadeManFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MadeManConfessionsOfTheFamilyBloodFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "Made Man: Confessions of the Family Blood";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		Util::stringcmp_caseless(exeName, "IMM.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_hipfireFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		spdlog_confparse(m_hipfireFOVFactor);
		spdlog_confparse(m_zoomFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 0B 89 0D ?? ?? ?? ?? 8B 53");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.ebx);
				s_instance_->m_newResY = Memory::ReadMem(ctx.ebx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "3D ?? ?? ?? ?? F3 0F 10 2D ?? ?? ?? ??");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult + 5 - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult + 5, 8);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult + 5, [](SafetyHookContext& ctx)
			{
				ctx.xmm5.f32[0] = s_instance_->m_newAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 3D ?? ?? ?? ?? D9 54 24 34 D8 4C 24 10 D9 5C 24 10 7E 43 0F 28 D0", "F3 0F 10 0D ?? ?? ?? ?? F3 0F 59 0D ?? ?? ?? ?? 83 48 0C 08",
		"F3 0F 10 05 ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? F3 0F 10 0D ?? ?? ?? ?? F3 0F 10 15 ?? ?? ?? ?? 74 05 8B 40 14",
		"F3 0F 10 05 ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? 5B F3 0F 11 40 48", "F3 0F 10 05 ?? ?? ?? ?? F3 0F 59 05 ?? ?? ?? ?? 83 48 0C 08 F3 0F 11 40 48 0F 57 C0 F3 0F 11 40 4C 8D 85 40 04 00 00", "F3 0F 10 43 4C 39 AE A0 01 00 00");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay FOV After Cutscenes Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GameplayAfterCutscenes1] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay FOV After Cutscenes Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GameplayAfterCutscenes2] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire1] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire2] - (std::uint8_t*)ExeModule());
			spdlog::info("Zoom FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 6);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGeneralFOV = 1.0f / s_instance_->m_aspectRatioScale;
				FPU::FDIVR(s_instance_->m_newGeneralFOV);
			});

			m_newHipfireFOV = m_originalHipfireFOV * m_hipfireFOVFactor;

			Memory::WriteNOPs(CameraFOVScansResult[GameplayAfterCutscenes1], 8);

			m_gameplayFOVAfterCutscenes1Hook = safetyhook::create_mid(CameraFOVScansResult[GameplayAfterCutscenes1], [](SafetyHookContext& ctx)
			{
				ctx.xmm1.f32[0] = s_instance_->m_newHipfireFOV;
			});

			Memory::WriteNOPs(CameraFOVScansResult[GameplayAfterCutscenes2], 8);

			m_gameplayFOVAfterCutscenes2Hook = safetyhook::create_mid(CameraFOVScansResult[GameplayAfterCutscenes2], [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = s_instance_->m_newHipfireFOV;
			});

			Memory::WriteNOPs(CameraFOVScansResult[Hipfire1], 8);

			m_hipfireFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Hipfire1], [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = s_instance_->m_newHipfireFOV;
			});

			Memory::WriteNOPs(CameraFOVScansResult[Hipfire2], 8);

			m_hipfireFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Hipfire2], [](SafetyHookContext& ctx)
			{
				ctx.xmm0.f32[0] = s_instance_->m_newHipfireFOV;
			});

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 5);

			m_weaponZoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWeaponZoomFOV = Memory::ReadMem(ctx.ebx + 0x4C);
				s_instance_->m_newWeaponZoomFOV = s_instance_->m_currentWeaponZoomFOV / s_instance_->m_zoomFactor;
				ctx.xmm0.f32[0] = s_instance_->m_newWeaponZoomFOV;
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalHipfireFOV = 65.0f;

	enum CameraFOVInstructionsIndices
	{
		General,
		GameplayAfterCutscenes1,
		GameplayAfterCutscenes2,
		Hipfire1,
		Hipfire2,
		Zoom
	};

	float m_hipfireFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;

	float m_newGeneralFOV = 0.0f;
	float m_newHipfireFOV = 0.0f;
	float m_currentWeaponZoomFOV = 0.0f;
	float m_newWeaponZoomFOV = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_gameplayFOVAfterCutscenes1Hook{};
	SafetyHookMid m_gameplayFOVAfterCutscenes2Hook{};
	SafetyHookMid m_hipfireFOV1Hook{};
	SafetyHookMid m_hipfireFOV2Hook{};
	SafetyHookMid m_weaponZoomFOVHook{};

	inline static MadeManFix* s_instance_ = nullptr;
};

static std::unique_ptr<MadeManFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<MadeManFix>(hModule);
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