#include "..\..\common\FixBase.hpp"

class GettysburgFix final : public FixBase
{
public:
	explicit GettysburgFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~GettysburgFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AmericanCivilWarGettysburgFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "American Civil War: Gettysburg";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Gettysburg.exe");
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
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "89 46 ?? E8 ?? ?? ?? ?? 89 46 ?? 8B 4F");
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
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 4A ?? D9 5E");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 3);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? 8B 44 24 ?? D8 0D ?? ?? ?? ?? 89 41",
		"D9 85 ?? ?? ?? ?? D9 85 ?? ?? ?? ?? D8 E1 D8 8C 24");
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
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_gameplayFOVHook{};

	enum CameraFOVInstructionsIndex
	{
		General,
		Gameplay
	};

	uint32_t m_currentWidth = 0;
	uint32_t m_currentHeight = 0;
	float m_zoomFactor = 0.0f;
	float m_newGeneralFOV = 0.0f;
	float m_newMaxGameplayFOV = 0.0f;
	float m_newMinimumGameplayFOV = 0.0f;

	void GameplayFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentMaxGameplayFOV = Memory::ReadMem(ctx.ebp + 0xE4);
		float& fCurrentMinimumGameplayFOV = Memory::ReadMem(ctx.ebp + 0xE8);
		m_newMaxGameplayFOV = fCurrentMaxGameplayFOV * m_fovFactor;
		m_newMinimumGameplayFOV = fCurrentMinimumGameplayFOV / m_zoomFactor;
		FPU::FLD(m_newMaxGameplayFOV);
		FPU::FLD(m_newMinimumGameplayFOV);
	}

	inline static GettysburgFix* s_instance_ = nullptr;
};

static std::unique_ptr<GettysburgFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<GettysburgFix>(hModule);
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