#include "..\..\common\FixBase.hpp"

class DromeRacersFix final : public FixBase
{
public:
	explicit DromeRacersFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DromeRacersFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DromeRacersFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Drome Racers";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Drome Racers.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{		
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 89 93 ?? ?? ?? ?? 89 83");
		if (ResolutionScanResult)
		{
			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x38);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x3C);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}

		auto CameraFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "D9 02 DE CB D9 CA", "D9 45 ?? D9 45 ?? D9 05 ?? ?? ?? ?? D9 05", "D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? DD 84 24");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[GeneralFOV] - (std::uint8_t*)ExeModule());

			spdlog::info("Min Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[MinFOV] - (std::uint8_t*)ExeModule());

			spdlog::info("Max Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[MaxFOV] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[GeneralFOV], 2);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[GeneralFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook(ctx.edx, 1.0f, s_instance_->m_aspectRatioScale, ctx);
			});

			// During races, minimum FOV is 70 and maximum is 125 in 4:3
			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MinFOV], 3);

			m_minimumFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MinFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook(ctx.ebp + 0x54, s_instance_->m_fovFactor, 1.0f, ctx);
			});

			MaxFOVAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[MaxFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[MaxFOV], 6);

			m_maxFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[MaxFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook(s_instance_->MaxFOVAddress, s_instance_->m_fovFactor, 1.0f, ctx);
			});
		}
	}

private:
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_minimumFOVHook{};
	SafetyHookMid m_maxFOVHook{};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	uintptr_t MaxFOVAddress = 0;

	enum CameraFOVInstructionsIndices
	{
		GeneralFOV,
		MinFOV,
		MaxFOV
	};

	void CameraFOVInstructionsMidHook(uintptr_t SourceAddress, float fovFactor, float arScale, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(SourceAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, arScale) * fovFactor;

		FPU::FLD(m_newCameraFOV);
	}

	inline static DromeRacersFix* s_instance_ = nullptr;
};

static std::unique_ptr<DromeRacersFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<DromeRacersFix>(hModule);
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