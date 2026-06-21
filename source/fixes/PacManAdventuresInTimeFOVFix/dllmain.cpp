#include "..\..\common\FixBase.hpp"

class PacManAdventuresInTimeFix final : public FixBase
{
public:
	explicit PacManAdventuresInTimeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PacManAdventuresInTimeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "PacManAdventuresInTimeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Pac-Man: Adventures in Time";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "pac-man.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 46 ?? A3 ?? ?? ?? ?? 8B 4E ?? 89 0D ?? ?? ?? ?? 8B 56 ?? 89 15 ?? ?? ?? ?? 8B 46 ?? A3 ?? ?? ?? ?? 8B 4E ?? 89 0D ?? ?? ?? ?? 8B 56 ?? 89 15 ?? ?? ?? ?? 8B 46 ?? A3 ?? ?? ?? ?? 8B 4E ?? 89 0D ?? ?? ?? ?? 8A 97");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esi + 0x14);
				int& iCurrentHeight = Memory::ReadMem(ctx.esi + 0x18);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 55");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 8);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio2 = 1.0f / s_instance_->m_newAspectRatio;

				*reinterpret_cast<float*>(ctx.esp + 0x34) = s_instance_->m_newAspectRatio2;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 68 ?? ?? ?? ?? B9", "68 ?? ?? ?? ?? 51 68 ?? ?? ?? ?? 8D 8E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult[FOV1] + 1, m_newCameraFOV);
			Memory::Write(CameraFOVScansResult[FOV2] + 1, m_newCameraFOV);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 1.0471975803375244f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	float m_currentCameraHFOV = 0.0f;
	float m_currentCameraVFOV = 0.0f;
	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	inline static PacManAdventuresInTimeFix* s_instance_ = nullptr;
};

static std::unique_ptr<PacManAdventuresInTimeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<PacManAdventuresInTimeFix>(hModule);
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