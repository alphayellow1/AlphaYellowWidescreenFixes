#include "..\..\common\FixBase.hpp"

class MoorhuhnKartXXLFix final : public FixBase
{
public:
	explicit MoorhuhnKartXXLFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MoorhuhnKartXXLFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MoorhuhnKartXXLWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Moorhuhn Kart XXL";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "MHK-XXL.exe");
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

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 56 ?? 8B 46 ?? 6A", "6A ?? 8B E8 FF D6");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[Res1], 6);

			m_resolution1Hook = safetyhook::create_mid(ResolutionScansResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			m_resolution2Hook = safetyhook::create_mid(ResolutionScansResult[Res2], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
				ctx.ebp = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 40 ?? ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? C3 90");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScanResult + 3, m_newAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 40 ?? ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? C3 90");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = Maths::CalculateNewFOV_RadBased(m_originalCameraFOV, m_aspectRatioScale) * m_fovFactor;

			Memory::Write(CameraFOVScanResult + 3, m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 16.0f / 10.0f;
	static constexpr float m_originalCameraFOV = 1.047166705f;

	SafetyHookMid m_resolution1Hook{};
	SafetyHookMid m_resolution2Hook{};

	enum ResolutionInstructionsIndices
	{
		Res1,
		Res2
	};

	inline static MoorhuhnKartXXLFix* s_instance_ = nullptr;
};

static std::unique_ptr<MoorhuhnKartXXLFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<MoorhuhnKartXXLFix>(hModule);
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