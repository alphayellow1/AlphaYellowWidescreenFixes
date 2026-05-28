#include "..\..\common\FixBase.hpp"

class ArcadeUSAFix final : public FixBase
{
public:
	explicit ArcadeUSAFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ArcadeUSAFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ArcadeUSAFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Arcade USA";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ArcadeUSA.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		GetAspectRatioFromIni();

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? 8B C6 5E 5D");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScanResult + 6, s_instance_->m_newAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 44 24 ?? 50 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 4C 24 ?? 51 8B CF E8 ?? ?? ?? ?? 8D 54 24 ?? 52 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 5F A3",
		"C7 86 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 44 24 ?? 50 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 4C 24 ?? 51 8B CF E8 ?? ?? ?? ?? 8D 54 24 ?? 52 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 5F 5E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Air Hockey Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[AirHockey] - (std::uint8_t*)ExeModule());
			spdlog::info("Bowling Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Bowling] - (std::uint8_t*)ExeModule());

			m_newAirHockeyFOV = m_originalAirHockeyFOV * m_fovFactor;
			m_newBowlingFOV = m_originalBowlingFOV * m_fovFactor;
			Memory::Write(CameraFOVScansResult[AirHockey] + 6, m_newAirHockeyFOV);
			Memory::Write(CameraFOVScansResult[Bowling] + 6, m_newBowlingFOV);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalAirHockeyFOV = 0.6981317401f;
	static constexpr float m_originalBowlingFOV = 0.6108652353f;

	float m_newAirHockeyFOV = 0.0f;
	float m_newBowlingFOV = 0.0f;

	enum CameraFOVInstructionsIndex
	{
		AirHockey,
		Bowling
	};

	void GetAspectRatioFromIni()
	{
		inipp::Ini<char> arcadeIni;
		std::ifstream iniFile("ARCADE.ini");
		arcadeIni.parse(iniFile);

		int iniWidth = 0;
		int iniHeight = 0;

		inipp::get_value(arcadeIni.sections["Video Card (Adapter 0)"], "Width", iniWidth);
		inipp::get_value(arcadeIni.sections["Video Card (Adapter 0)"], "Height", iniHeight);

		m_newAspectRatio = static_cast<float>(iniWidth) / static_cast<float>(iniHeight);
	}

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	inline static ArcadeUSAFix* s_instance_ = nullptr;
};

static std::unique_ptr<ArcadeUSAFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ArcadeUSAFix>(hModule);
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