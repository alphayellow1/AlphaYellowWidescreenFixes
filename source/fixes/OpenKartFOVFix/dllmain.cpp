#include "..\..\common\FixBase.hpp"

class OpenKartFix final : public FixBase
{
public:
	explicit OpenKartFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~OpenKartFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "OpenKartFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Open Kart";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Kart.exe") ||
		Util::stringcmp_caseless(exeName, "KartDemo.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 4D ?? 8B 53 ?? 50 51");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.ebp + 0xC);
				s_instance_->m_newResY = Memory::ReadMem(ctx.ebp + 0x8);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? D9 5C 24",
		"C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 8B 45");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay HFOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay HFOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] + 22 - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay VFOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] + 38 - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay VFOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] + 54 - (std::uint8_t*)ExeModule());
			spdlog::info("Kart Setup HFOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[KartSetup] - (std::uint8_t*)ExeModule());
			spdlog::info("Kart Setup HFOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[KartSetup] + 10 - (std::uint8_t*)ExeModule());
			
			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 6);

			m_gameplayHFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGameplayHFOV1 = m_originalGameplayFOV1 * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				FPU::FMUL(s_instance_->m_newGameplayHFOV1);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay] + 22, 6);

			m_gameplayHFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay] + 22, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGameplayHFOV2 = m_originalGameplayFOV2 * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				FPU::FMUL(s_instance_->m_newGameplayHFOV2);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay] + 38, 6);

			m_gameplayVFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay] + 38, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGameplayVFOV1 = m_originalGameplayFOV2 * s_instance_->m_fovFactor;
				FPU::FMUL(s_instance_->m_newGameplayVFOV1);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay] + 54, 6);

			m_gameplayVFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay] + 54, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newGameplayVFOV2 = m_originalGameplayFOV1 * s_instance_->m_fovFactor;
				FPU::FMUL(s_instance_->m_newGameplayVFOV2);
			});

			Memory::WriteNOPs(CameraFOVScansResult[KartSetup], 20);

			m_kartSetupHFOVHook = safetyhook::create_mid(CameraFOVScansResult[KartSetup], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newKartSetupHFOV1 = m_originalKartSetupHFOV1 * s_instance_->m_aspectRatioScale;
				s_instance_->m_newKartSetupHFOV2 = m_originalKartSetupHFOV2 * s_instance_->m_aspectRatioScale;
				*reinterpret_cast<float*>(ctx.ecx + 0x144) = s_instance_->m_newKartSetupHFOV1;
				*reinterpret_cast<float*>(ctx.ecx + 0x148) = s_instance_->m_newKartSetupHFOV2;
			});
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "6A ?? 68 ?? ?? ?? ?? 50 8D 4C 24 ?? E8 ?? ?? ?? ?? 6A",
			"6A ?? 68 ?? ?? ?? ?? 50 8D 4C 24 ?? E8 ?? ?? ?? ?? 8B 4C 24");
			if (Memory::AreAllSignaturesValid(SkipIntroVideosScansResult) == true)
			{
				spdlog::info("Skip Intro Video Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipVids1] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Video Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[SkipVids2] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(SkipIntroVideosScansResult[SkipVids1], 17);
				Memory::WriteNOPs(SkipIntroVideosScansResult[SkipVids2], 17);
			}			
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalGameplayFOV1 = -0.5f;
	static constexpr float m_originalGameplayFOV2 = 0.5f;
	static constexpr float m_originalKartSetupHFOV1 = -0.5500000119f;
	static constexpr float m_originalKartSetupHFOV2 = 0.5500000119f;

	bool m_skipIntroVideos = false;

	float m_newGameplayHFOV1 = 0.0f;	
	float m_newGameplayHFOV2 = 0.0f;
	float m_newGameplayVFOV1 = 0.0f;
	float m_newGameplayVFOV2 = 0.0f;
	float m_newKartSetupHFOV1 = 0.0f;
	float m_newKartSetupHFOV2 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_gameplayHFOV1Hook{};
	SafetyHookMid m_gameplayHFOV2Hook{};
	SafetyHookMid m_gameplayVFOV1Hook{};
	SafetyHookMid m_gameplayVFOV2Hook{};
	SafetyHookMid m_kartSetupHFOVHook{};

	enum CameraFOVInstructionsIndex
	{
		Gameplay,
		KartSetup
	};

	enum SkipIntroVideosScansIndex
	{
		SkipVids1,
		SkipVids2
	};

	inline static OpenKartFix* s_instance_ = nullptr;
};

static std::unique_ptr<OpenKartFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<OpenKartFix>(hModule);
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