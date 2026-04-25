#include "..\..\common\FixBase.hpp"

class DalmatiansFix final : public FixBase
{
public:
	explicit DalmatiansFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DalmatiansFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "102DalmatiansPuppiesToTheRescueFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "102 Dalmatians: Puppies to the Rescue";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Pcdogs.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "0D 00 00 00 0F 00 00 00 10 00 00 00 FF 00 00 00 00 10 00 00 00 00 00 00 00 10 00 00 00");
		if (!AspectRatioScanResult)
		{
			spdlog::error("Failed to locate aspect ratio memory address.");
			return;
		}

		spdlog::info("Aspect Ratio: Address is {}+{:x}", ExeName().c_str(), AspectRatioScanResult + 16 - reinterpret_cast<std::uint8_t*>(ExeModule()));

		iNewAspectRatio = (int)(iOriginalAspectRatio / m_aspectRatioScale);

		Memory::Write(AspectRatioScanResult + 16, iNewAspectRatio);
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr uint16_t iOriginalAspectRatio = 4096;
	static inline int iNewAspectRatio;

	inline static DalmatiansFix* s_instance_ = nullptr;
};

static std::unique_ptr<DalmatiansFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<DalmatiansFix>(hModule);

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