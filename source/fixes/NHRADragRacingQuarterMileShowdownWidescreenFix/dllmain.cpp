#include "..\..\common\FixBase.hpp"

class NHRAFix final : public FixBase
{
public:
	explicit NHRAFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~NHRAFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "NHRADragRacingQuarterMileShowdownWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.0";
	}

	const char* TargetName() const override
	{
		return "NHRA Drag Racing: Quarter Mile Showdown";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "NHRAPC.exe");
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

		auto ResolutionListsScansResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 33 DB",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C3 C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C3 C7 05");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[ResList1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[ResList2] - (std::uint8_t*)ExeModule());
			
			// Resolution List 1
			// 640x480
			Memory::Write(ResolutionListsScansResult[ResList1] + 4, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 28, m_newResY);
			// 800x600
			Memory::Write(ResolutionListsScansResult[ResList1] + 12, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 36, m_newResY);
			// 1024x768
			Memory::Write(ResolutionListsScansResult[ResList1] + 20, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 44, m_newResY);

			// Resolution List 2
			// 640x480
			Memory::Write(ResolutionListsScansResult[ResList2] + 48, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 58, m_newResY);
			// 800x600
			Memory::Write(ResolutionListsScansResult[ResList2] + 27, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 37, m_newResY);
			// 1024x768
			Memory::Write(ResolutionListsScansResult[ResList2] + 6, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 16, m_newResY);
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? EB ?? A1");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::WriteNOPs(CameraFOVScanResult + 1, m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_originalCameraFOV = 0.5f;

	enum ResolutionListsIndex
	{
		ResList1,
		ResList2
	};

	inline static NHRAFix* s_instance_ = nullptr;
};

static std::unique_ptr<NHRAFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<NHRAFix>(hModule);
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