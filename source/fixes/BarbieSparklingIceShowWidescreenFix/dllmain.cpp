#include "..\..\common\FixBase.hpp"

class BarbieSparklingIceShowFix final : public FixBase
{
public:
	explicit BarbieSparklingIceShowFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BarbieSparklingIceShowFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BarbieSparklingIceShowWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Barbie: Sparking Ice Show";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "IceSkating.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "Framerate", m_newFramerate);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_newFramerate);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? C7 05", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? A1",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 68", "81 FE ?? ?? ?? ?? 89 74 24");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Initialization Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Init] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Configuration 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Config1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Configuration 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Config2] - (std::uint8_t*)ExeModule());
			spdlog::info("Tile/Grid Drawing Limit Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[TileDraw] - (std::uint8_t*)ExeModule());

			Memory::Write(ResolutionScansResult[Init] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Init] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Config1] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Config1] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[Config2] + 6, m_newResX);
			Memory::Write(ResolutionScansResult[Config2] + 1, m_newResY);
			Memory::Write(ResolutionScansResult[TileDraw] + 2, m_newResX);
			Memory::Write(ResolutionScansResult[TileDraw] + 17, m_newResY);
		}
		
		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D8 8E");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScanResult + 2, &m_newAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D8 3D ?? ?? ?? ?? D9 C0 D8 8E");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = (1.0f / m_aspectRatioScale) / m_fovFactor;

			Memory::Write(CameraFOVScanResult + 2, &m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
		
		auto FramerateScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? A3 ?? ?? ?? ?? 89 0D");
		if (FramerateScanResult)
		{
			spdlog::info("Framerate Instruction: Address is {:s}+{:x}", ExeName().c_str(), FramerateScanResult - (std::uint8_t*)ExeModule());
			spdlog::info("Framerate Delta Instruction: Address is {:s}+{:x}", ExeName().c_str(), FramerateScanResult + 10 - (std::uint8_t*)ExeModule());

			m_newFramerateDelta = 1.0f / m_newFramerate;

			Memory::Write(FramerateScanResult + 6, m_newFramerate);
			Memory::Write(FramerateScanResult + 16, m_newFramerateDelta);
		}
		else
		{
			spdlog::error("Failed to locate framerate instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newFramerate = 0.0f;
	float m_newFramerateDelta = 0.0f;

	enum ResolutionInstructionsIndex
	{
		Init,
		Config1,
		Config2,
		TileDraw
	};

	inline static BarbieSparklingIceShowFix* s_instance_ = nullptr;
};

static std::unique_ptr<BarbieSparklingIceShowFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BarbieSparklingIceShowFix>(hModule);
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