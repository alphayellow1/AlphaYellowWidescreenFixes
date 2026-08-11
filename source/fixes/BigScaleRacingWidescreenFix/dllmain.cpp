#include "..\..\common\FixBase.hpp"

class BigScaleRacingFix final : public FixBase
{
public:
	explicit BigScaleRacingFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BigScaleRacingFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BigScaleRacingWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Big Scale Racing";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Big Scale Racing.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroLogos", m_skipIntroLogos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroLogos);
	}

	void ApplyFix() override
	{
		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "75 ?? 81 7E ?? ?? ?? ?? ?? 74 ?? 81 FF ?? ?? ?? ?? 75 ?? 81 7E ?? ?? ?? ?? ?? 74",
		"8B 48 ?? 89 8E ?? ?? ?? ?? 8B 50");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(m_resolutionScansResult[ListUnlock], "\xEB\x2B");

			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.eax + 0x18);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x1C);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 0D ?? ?? ?? ?? 51 D9 F2 DD D8 DC C0 D9 1C ?? D9 44 24",
		"89 86 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 46");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV1], 4);

			m_cameraFOV1Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newCameraFOV1);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV2], 6);

			m_cameraFOV2Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				const float& fCurrentCameraFOV2 = Memory::ReadRegister(ctx.eax);
				s_instance_->m_newCameraFOV2 = fCurrentCameraFOV2 * s_instance_->m_fovFactor;
				*reinterpret_cast<float*>(ctx.esi + 0xB8) = s_instance_->m_newCameraFOV2;
			});
		}

		m_altTabFixScansResult = Memory::PatternScan(ExeModule(), "6A ?? EB ?? 6A ?? 50 51 FF 52 ?? 85 C0 A3 ?? ?? ?? ?? 7D ?? 5F 32 C0 5E C2 ?? ?? 8B 86",
		"6A ?? 50 51 FF 52 ?? 85 C0 A3 ?? ?? ?? ?? 7D ?? 5F 32 C0 5E C2 ?? ?? 8B 86");
		if (Memory::AreAllSignaturesValid(m_altTabFixScansResult) == true)
		{
			spdlog::info("Alt+Tab Fix Scan 1: Address is {:s}+{:x}", ExeName().c_str(), m_altTabFixScansResult[AltTab1] - (std::uint8_t*)ExeModule());
			spdlog::info("Alt+Tab Fix Scan 2: Address is {:s}+{:x}", ExeName().c_str(), m_altTabFixScansResult[AltTab2] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(m_altTabFixScansResult[AltTab1] + 1, "\x06");
			Memory::PatchBytes(m_altTabFixScansResult[AltTab2] + 1, "\x06");
		}

		if (m_skipIntroLogos == true)
		{
			m_skipIntroLogosScansResult = Memory::PatternScan(ExeModule(), "89 9E ?? ?? ?? ?? EB ?? C6 44 24", "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? C6 44 24", "C7 86 ?? ?? ?? ?? ?? ?? ?? ?? 8B 4E ?? 3B CB 74");
			if (Memory::AreAllSignaturesValid(m_skipIntroLogosScansResult))
			{
				spdlog::info("Loading Screen 1 State Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroLogosScansResult[LoadingScreen1State] - (std::uint8_t*)(ExeModule()));
				spdlog::info("Loading Screen 2 State Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroLogosScansResult[LoadingScreen2State] - (std::uint8_t*)(ExeModule()));
				spdlog::info("Loading Screen 3 State Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroLogosScansResult[LoadingScreen3State] - (std::uint8_t*)(ExeModule()));

				Memory::WriteNOPs(m_skipIntroLogosScansResult[LoadingScreen1State], 6);

				m_skipIntroLogosHook = safetyhook::create_mid(m_skipIntroLogosScansResult[LoadingScreen1State], [](SafetyHookContext & ctx)
				{
					*reinterpret_cast<float*>(ctx.esi + 0x2B77C) = 4.0f;
				});

				Memory::Write(m_skipIntroLogosScansResult[LoadingScreen2State] + 6, 8.0f);
				Memory::Write(m_skipIntroLogosScansResult[LoadingScreen3State] + 6, 13.0f);
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::vector<std::uint8_t*> m_altTabFixScansResult{};
	std::vector<std::uint8_t*> m_skipIntroLogosScansResult{};

	bool m_skipIntroLogos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_skipIntroLogosHook{};

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
	};

	enum CameraHFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	enum AltTabFixInstructionsIndices
	{
		AltTab1,
		AltTab2
	};

	enum SkipIntroLogosScanIndex
	{
		LoadingScreen1State,
		LoadingScreen2State,
		LoadingScreen3State
	};

	inline static BigScaleRacingFix* s_instance_ = nullptr;
};

static std::unique_ptr<BigScaleRacingFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<BigScaleRacingFix>(hModule);
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