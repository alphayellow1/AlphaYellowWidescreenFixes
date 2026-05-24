#include "..\..\common\FixBase.hpp"

class MotoX2000Fix final : public FixBase
{
public:
	explicit MotoX2000Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MotoX2000Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EdgarTorronterasMotoX2000WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Edgar Torronteras' Moto-X 2000";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "MotoX2000.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "0F 82 ?? ?? ?? ?? 3D ?? ?? ?? ?? 0F 87 ?? ?? ?? ?? 81 F9", "8B 74 24 ?? 8B 7C 24 ?? 8B D6");
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
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0xC);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x10);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteFOV();
			});
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 0D", "68 ?? ?? ?? ?? 51 C7 05");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Menu Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Menu] - (std::uint8_t*)ExeModule());
			spdlog::info("Races Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Races] - (std::uint8_t*)ExeModule());
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalMenuFOV = 60.0f;
	static constexpr float m_originalRacesFOV = 90.0f;

	SafetyHookMid m_resolutionHook{};

	std::vector<std::uint8_t*> CameraFOVScansResult;

	float m_newMenuFOV = 0.0f;
	float m_newRacesFOV = 0.0f;

	enum ResolutionInstructionsIndices
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum CameraFOVInstructionsScansIndices
	{
		Menu,
		Races
	};

	void WriteFOV()
	{
		m_newMenuFOV = Maths::CalculateNewFOV_DegBased(m_originalMenuFOV, m_aspectRatioScale);
		m_newRacesFOV = Maths::CalculateNewFOV_DegBased(m_originalRacesFOV, m_aspectRatioScale) * m_fovFactor;

		Memory::Write(CameraFOVScansResult[Menu] + 1, m_newMenuFOV);
		Memory::Write(CameraFOVScansResult[Races] + 1, m_newRacesFOV);
	}

	inline static MotoX2000Fix* s_instance_ = nullptr;
};

static std::unique_ptr<MotoX2000Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<MotoX2000Fix>(hModule);
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