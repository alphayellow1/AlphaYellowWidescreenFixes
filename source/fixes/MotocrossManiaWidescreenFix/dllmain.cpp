#include "..\..\common\FixBase.hpp"

class MotocrossManiaFix final : public FixBase
{
public:
	explicit MotocrossManiaFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MotocrossManiaFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MotocrossManiaWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Motocross Mania";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "MxMania.exe") ||
		Util::stringcmp_caseless(exeName, "MxManiaUS.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "0F 82 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 87 ?? ?? ?? ?? 81 F9", "8B 75 ?? 8B 7D ?? 8B D6");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 11, 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 23, 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 35, 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 56, 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 77, 6);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.ebp + 0x8);
				int& iCurrentHeight = Memory::ReadMem(ctx.ebp + 0xC);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteStaticFOVs();
			});
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50", "8B 44 24 ?? 50 8B 84 24",
		"C7 44 24 ?? ?? ?? ?? ?? 0F 85");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Menu Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Menu] - (uint8_t*)ExeModule());
			spdlog::info("Races Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Races1] - (uint8_t*)ExeModule());
			spdlog::info("Races Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Races2] - (uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[Races1], 4);

			m_racesFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Races1], [](SafetyHookContext& ctx)
			{
				float& fCurrentRacesFOV1 = Memory::ReadMem(ctx.esp + 0x28);
				s_instance_->m_newRacesFOV1 = Maths::CalculateNewFOV_DegBased(fCurrentRacesFOV1, s_instance_->m_aspectRatioScale);
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newRacesFOV1);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalMenuFOV = 60.0f;
	static constexpr float m_originalRacesFOV2 = 85.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_racesFOV1Hook{};

	std::vector<std::uint8_t*> CameraFOVScansResult;

	float m_newMenuFOV = 0.0f;
	float m_newRacesFOV1 = 0.0f;
	float m_newRacesFOV2 = 0.0f;

	enum ResolutionInstructionsIndices
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum CameraFOVInstructionsScansIndices
	{
		Menu,
		Races1,
		Races2
	};

	void WriteStaticFOVs()
	{
		m_newMenuFOV = Maths::CalculateNewFOV_DegBased(m_originalMenuFOV, m_aspectRatioScale);
		m_newRacesFOV2 = m_originalRacesFOV2 * m_fovFactor;

		Memory::Write(CameraFOVScansResult[Menu] + 1, m_newMenuFOV);
		Memory::Write(CameraFOVScansResult[Races2] + 4, m_newRacesFOV2);
	}

	inline static MotocrossManiaFix* s_instance_ = nullptr;
};

static std::unique_ptr<MotocrossManiaFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<MotocrossManiaFix>(hModule);
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