#include "..\..\common\FixBase.hpp"

class ExtremeSportsFix final : public FixBase
{
public:
	explicit ExtremeSportsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ExtremeSportsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ExtremeSportsWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Extreme Sports";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "pc.exe");
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

		auto ResolutionListScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 68");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution list Scan: Address is{:s} + {:x}", ExeName().c_str(), ResolutionListScanResult - (std::uint8_t*)ExeModule());

			// 800x600
			Memory::Write(ResolutionListScanResult + 6, m_newResX);
			Memory::Write(ResolutionListScanResult + 16, m_newResY);

			// 640x480
			Memory::Write(ResolutionListScanResult + 28, m_newResX);
			Memory::Write(ResolutionListScanResult + 38, m_newResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? DB 44 24 ?? D8 0D ?? ?? ?? ?? DE F9");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			m_newAspectRatio2 = m_originalAspectRatio * m_aspectRatioScale;

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 40 ?? D8 4B ?? D9 1C", "8B 0D ?? ?? ?? ?? 50 8D 96 ?? ?? ?? ?? 57");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 3);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = Memory::ReadMem(ctx.eax + 0x40);
				s_instance_->m_newGeneralFOV = fCurrentGeneralFOV * s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newGeneralFOV);
			});

			Sleep(2000);

			m_gameplayFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[Gameplay] + 2, Memory::PointerMode::Absolute);

			m_newGameplayFOV = m_originalGameplayFOV * m_fovFactor;

			Memory::Write(m_gameplayFOVAddress, m_newGameplayFOV);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalAspectRatio = 4.0f;
	static constexpr float m_originalGameplayFOV = 1.4281479f;

	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_generalFOVHook{};

	enum CameraFOVInstructionsIndex
	{
		General,
		Gameplay
	};

	uintptr_t m_gameplayFOVAddress = 0;
	float m_newGeneralFOV = 0.0f;
	float m_newGameplayFOV = 0.0f;

	inline static ExtremeSportsFix* s_instance_ = nullptr;
};

static std::unique_ptr<ExtremeSportsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ExtremeSportsFix>(hModule);
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