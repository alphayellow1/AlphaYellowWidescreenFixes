#include "..\..\common\FixBase.hpp"

class BNABFix final : public FixBase
{
public:
	explicit BNABFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BNABFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BeyondNormandyAssignmentBerlinWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "Beyond Normandy: Assignment Berlin";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AssignmentBerlin-V1.05.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "HUDMargin", m_hudMargin);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_hudMargin);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionListsScansResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 50",
		"C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 B4 24");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[List1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[List2] - (std::uint8_t*)ExeModule());

			Memory::Write(ResolutionListsScansResult[List1] + 4, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 12, m_newResY);

			Memory::Write(ResolutionListsScansResult[List2] + 4, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 12, m_newResY);
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? 74 ?? D9 86");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = 0.8f * m_aspectRatioScale * m_fovFactor;

			Memory::Write(CameraFOVScanResult + 4, m_newCameraFOV);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}

		auto HUDScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? D9 44 24",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 8B C8", "D8 0D ?? ?? ?? ?? D8 44 24 ?? D9 5C 24 ?? D9 44 24",
		"D8 0D ?? ?? ?? ?? 51 8D 4C 24 ?? D9 1C 24", "DC 0D ?? ?? ?? ?? D9 C9 D9 C9 DE C9 D8 0D ?? ?? ?? ?? D8 44 24", "D8 0D ?? ?? ?? ?? 50 51 8D 4C 24 ?? D9 5C 24 ?? 8B 44 24",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? 8B 4C 24 ?? D8 0D ?? ?? ?? ?? D9 54 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24 ?? D8 0D ?? ?? ?? ?? D8 2D",
		"DC 0D ?? ?? ?? ?? D9 58 ?? D9 44 24 ?? DC 0D ?? ?? ?? ?? D9 58", "D8 0D ?? ?? ?? ?? 50 51 8D 4C 24 ?? D9 5C 24 ?? D9 44 24",
		"D8 0D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? 8B 4C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24", "D8 0D ?? ?? ?? ?? 8B 44 24 ?? 50", "D8 0D ?? ?? ?? ?? D8 44 24 ?? D9 5C 24 ?? D9 44 24");
		if (Memory::AreAllSignaturesValid(HUDScansResult) == true)
		{
			spdlog::info("(HUD) Loading Bar X Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD1] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Red Cross and Menu Text Width Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD2] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Left Margin: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD3] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Compass Needle X: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD4] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Objective Direction Width: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD5] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Compass X: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD6] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Compass Width: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD7] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Weapons List Width: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD8] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Text Width: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD9] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Ammo X: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD10] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Ammo Width: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD11] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Compass Needle Width: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD12] - (std::uint8_t*)ExeModule());
			spdlog::info("(HUD) Unknown Element 1 Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD13] - (std::uint8_t*)ExeModule());

			m_loadingBarXAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD1] + 2, Memory::PointerMode::Absolute);
			m_redCrossAndMenuTextWidthAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD2] + 2, Memory::PointerMode::Absolute);
			m_leftMarginAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD3] + 2, Memory::PointerMode::Absolute);
			m_compassNeedleXAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD4] + 2, Memory::PointerMode::Absolute);
			m_objectiveDirectionWidthAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD5] + 2, Memory::PointerMode::Absolute);
			m_compassXAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD6] + 2, Memory::PointerMode::Absolute);
			m_compassWidthAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD7] + 2, Memory::PointerMode::Absolute);
			m_weaponsListWidthAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD8] + 2, Memory::PointerMode::Absolute);
			m_textWidthAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD9] + 2, Memory::PointerMode::Absolute);
			m_ammoXAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD10] + 2, Memory::PointerMode::Absolute);
			m_ammoWidthAddress = Memory::GetPointerFromAddress(HUDScansResult[HUD11] + 2, Memory::PointerMode::Absolute);

			m_hudWidth = 600.0f * m_newAspectRatio;

			// Loading bar X
			m_loadingBarX = 222.0f + (m_hudWidth - 800.0f) / 2.0f;
			Memory::Write(m_loadingBarXAddress, m_loadingBarX);

			// Red cross and menu text width
			m_redCrossAndMenuTextWidth = 0.001666f / m_newAspectRatio;
			Memory::Write(m_redCrossAndMenuTextWidthAddress, m_redCrossAndMenuTextWidth);

			// HUD left margin
			m_hudLeftMargin = 0.0333333f / m_newAspectRatio + m_hudMargin / m_hudWidth;
			Memory::Write(m_leftMarginAddress, m_hudLeftMargin);

			// Compass needle X
			m_compassNeedleX = (m_hudWidth - 79.0f - m_hudMargin) / m_hudWidth;
			Memory::Write(m_compassNeedleXAddress, m_compassNeedleX);

			// Objective direction width
			m_objectiveDirectionWidth = 0.001666 / static_cast<double>(m_newAspectRatio);
			Memory::Write(m_objectiveDirectionWidthAddress, m_objectiveDirectionWidth);

			// Compass X
			m_compassX = (m_hudWidth - 118.0f - m_hudMargin) / m_hudWidth;
			Memory::Write(m_compassXAddress, m_compassX);

			// Compass width
			m_compassWidth = 0.163333f / m_newAspectRatio;
			Memory::Write(m_compassWidthAddress, m_compassWidth);

			// Weapons list width
			m_weaponsListWidth = 0.17f / m_newAspectRatio;
			Memory::Write(m_weaponsListWidthAddress, m_weaponsListWidth);

			// HUD text width
			m_hudTextWidth = 0.0006666666666 / static_cast<double>(m_newAspectRatio);
			Memory::Write(m_textWidthAddress, m_hudTextWidth);

			// Ammo X
			m_ammoX = (m_hudWidth - 74.0f - m_hudMargin) / m_hudWidth;
			Memory::Write(m_ammoXAddress, m_ammoX);

			// Ammo width
			m_ammoWidth = 0.116666f / m_newAspectRatio;
			Memory::Write(m_ammoWidthAddress, m_ammoWidth);

			// Compass needle width
			m_compassNeedleWidth = 0.0333333f / m_newAspectRatio;
			Memory::Write(HUDScansResult[HUD12] + 2, &m_compassNeedleWidth);

			// Unknown HUD element #1
			m_unknownHUDElement1 = 0.025000000372529f;
			Memory::Write(HUDScansResult[HUD13] + 2, &m_unknownHUDElement1);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	uintptr_t m_loadingBarXAddress = 0;
	uintptr_t m_redCrossAndMenuTextWidthAddress = 0;
	uintptr_t m_leftMarginAddress = 0;
	uintptr_t m_compassNeedleXAddress = 0;
	uintptr_t m_objectiveDirectionWidthAddress = 0;
	uintptr_t m_compassXAddress = 0;
	uintptr_t m_compassWidthAddress = 0;
	uintptr_t m_weaponsListWidthAddress = 0;
	uintptr_t m_textWidthAddress = 0;
	uintptr_t m_ammoXAddress = 0;
	uintptr_t m_ammoWidthAddress = 0;

	float m_hudMargin;
	float m_compassNeedleWidth;
	float m_loadingBarX;
	float m_redCrossAndMenuTextWidth;
	float m_hudLeftMargin;
	float m_hudWidth;
	float m_compassNeedleX;
	float m_compassX;
	float m_compassWidth;
	float m_weaponsListWidth;
	float m_ammoX;
	float m_ammoWidth;
	float m_unknownHUDElement1;
	double m_objectiveDirectionWidth;
	double m_hudTextWidth;

	enum ResolutionInstructionsIndices
	{
		List1,
		List2
	};

	enum HUDElementsIndices
	{
		HUD1,
		HUD2,
		HUD3,
		HUD4,
		HUD5,
		HUD6,
		HUD7,
		HUD8,
		HUD9,
		HUD10,
		HUD11,
		HUD12,
		HUD13
	};

	inline static BNABFix* s_instance_ = nullptr;
};

static std::unique_ptr<BNABFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BNABFix>(hModule);
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