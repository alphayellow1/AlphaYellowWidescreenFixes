#include "..\..\common\FixBase.hpp"

class CrazyGolfFix final : public FixBase
{
public:
	explicit CrazyGolfFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~CrazyGolfFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "CrazyGolfFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Crazy Golf";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "CrazyGolf.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		GetResolutionFromIni();

		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? 8B C6 5E 5D 89 54 24", "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? D9 96");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScansResult, AR1, AR2, 6, m_newAspectRatio);
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 8F");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScanResult + 6, m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction scan memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 1.04719758f;

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2
	};

	void GetResolutionFromIni()
	{
		inipp::Ini<char> arcadeIni;
		std::ifstream iniFile("CrazyGolf.ini");
		arcadeIni.parse(iniFile);

		inipp::get_value(arcadeIni.sections["Video Card (Adapter 0)"], "Width", m_newResX);
		inipp::get_value(arcadeIni.sections["Video Card (Adapter 0)"], "Height", m_newResY);
	}

	inline static CrazyGolfFix* s_instance_ = nullptr;
};

static std::unique_ptr<CrazyGolfFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<CrazyGolfFix>(hModule);
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