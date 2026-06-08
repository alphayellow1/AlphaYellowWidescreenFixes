#include "..\..\common\FixBase.hpp"

class SchoolTycoonFix final : public FixBase
{
public:
	explicit SchoolTycoonFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SchoolTycoonFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SchoolTycoonFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "School Tycoon";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "SchoolTycoon.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_zoomFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "89 46 ?? E8 ?? ?? ?? ?? 89 46 ?? D9 47");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadRegister(ctx.eax);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionScanResult + 8, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHeight = Memory::ReadRegister(ctx.eax);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteStaticARs();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 8B CF E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF E8 ?? ?? ?? ?? 8B 4C 24",
		"68 ?? ?? ?? ?? 8B CD E8 ?? ?? ?? ?? A1", "68 ?? ?? ?? ?? 8B CF E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF E8 ?? ?? ?? ?? 56");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? 8B 44 24 ?? D8 0D ?? ?? ?? ?? 89 41",
		"D9 85 ?? ?? ?? ?? D9 85 ?? ?? ?? ?? D8 E1 83 C4");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 4);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = Memory::ReadMem(ctx.esp + 0x4);
				s_instance_->m_newGeneralFOV = Maths::CalculateNewFOV_RadBased(fCurrentGeneralFOV, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newGeneralFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 12);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->GameplayFOVMidHook(ctx);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_gameplayFOVHook{};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3
	};

	enum CameraFOVInstructionsIndex
	{
		General,
		Gameplay
	};

	std::vector<std::uint8_t*> AspectRatioScansResult;

	uint32_t m_currentWidth = 0;
	uint32_t m_currentHeight = 0;
	float m_zoomFactor = 0.0f;
	float m_newGeneralFOV = 0.0f;
	float m_newMaxGameplayFOV = 0.0f;
	float m_newMinimumGameplayFOV = 0.0f;

	void WriteStaticARs()
	{
		Memory::Write(AspectRatioScansResult, AR1, AR3, 1, m_newAspectRatio);
	}

	void GameplayFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentMinGameplayFOV = Memory::ReadMem(ctx.ebp + 0x114);
		float& fCurrentMaxGameplayFOV = Memory::ReadMem(ctx.ebp + 0x118);		
		m_newMinimumGameplayFOV = fCurrentMinGameplayFOV / m_zoomFactor;
		m_newMaxGameplayFOV = fCurrentMaxGameplayFOV * m_fovFactor;
		FPU::FLD(m_newMinimumGameplayFOV);
		FPU::FLD(m_newMaxGameplayFOV);
	}

	inline static SchoolTycoonFix* s_instance_ = nullptr;
};

static std::unique_ptr<SchoolTycoonFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<SchoolTycoonFix>(hModule);
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