#include "..\..\common\FixBase.hpp"

class WoSSteelAcrossAmericaFix final : public FixBase
{
public:
	explicit WoSSteelAcrossAmericaFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WoSSteelAcrossAmericaFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "18WheelsOfSteelAcrossAmericaFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "18 Wheels of Steel: Across America";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "prism3d.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		gameDll = Memory::GetHandle("game.dll");

		gameDllName = Memory::GetModuleName(gameDll);

		auto CameraFOVScansResult = Memory::PatternScan(gameDll, "C7 44 24 ?? ?? ?? ?? ?? C6 44 24 ?? ?? FF 15 ?? ?? ?? ?? FF 15",
		"C7 44 24 ?? ?? ?? ?? ?? D9 15");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)gameDll);

			m_newCameraFOV = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV, m_aspectRatioScale);

			Memory::Write(CameraFOVScansResult[FOV1] + 4, m_newCameraFOV);

			Memory::Write(CameraFOVScansResult[FOV2] + 4, m_newCameraFOV * m_fovFactor);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	HMODULE gameDll = nullptr;

	std::string gameDllName;

	enum CameraFOVScanResult
	{
		FOV1,
		FOV2
	};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	static constexpr float m_originalCameraFOV = 70.0f;

	inline static WoSSteelAcrossAmericaFix* s_instance_ = nullptr;
};

static std::unique_ptr<WoSSteelAcrossAmericaFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<WoSSteelAcrossAmericaFix>(hModule);

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