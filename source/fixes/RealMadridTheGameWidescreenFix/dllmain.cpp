#include "..\..\common\FixBase.hpp"

class RealMadridTheGameFix final : public FixBase
{
public:
	explicit RealMadridTheGameFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RealMadridTheGameFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "RealMadridTheGameWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Real Madrid: The Game";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Game.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 4C 24 ?? 8A 54 24 ?? 56", "8B 88 ?? ?? ?? ?? 8B 90 ?? ?? ?? ?? 89 0D",
		"A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 89 15");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[Res1], 4);

			m_resolutionWidth1Hook = safetyhook::create_mid(ResolutionScansResult[Res1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			Memory::WriteNOPs(ResolutionScansResult[Res1] + 9, 4);

			m_resolutionHeight1Hook = safetyhook::create_mid(ResolutionScansResult[Res1] + 9, [](SafetyHookContext& ctx)
			{
				ctx.esi = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::WriteNOPs(ResolutionScansResult[Res2], 12);

			m_resolution2Hook = safetyhook::create_mid(ResolutionScansResult[Res2], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});

			Memory::WriteNOPs(ResolutionScansResult[Res3], 11);

			m_resolution3Hook = safetyhook::create_mid(ResolutionScansResult[Res3], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? 51 8D 4C 24 ?? D9 1C 24 E8", "D9 42 ?? 51 8B CE");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			m_newCameraFOV1 = m_originalCameraFOV1 * m_fovFactor;

			Memory::Write(CameraFOVScansResult[FOV1] + 2, &m_newCameraFOV1);

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 3);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.edx + 0x20);
				s_instance_->m_newCameraFOV2 = fCurrentCameraFOV2 * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV2);
			});
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? A1 ?? ?? ?? ?? 50 FF 15");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "32 DB E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 85 C0");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - reinterpret_cast<std::uint8_t*>(ExeModule()));

				m_skipIntroVideosHook = safetyhook::create_mid(SkipIntroVideosScanResult + 612, [](SafetyHookContext& ctx)
				{
					const auto videoObject = static_cast<std::uintptr_t>(ctx.esi);

					if (!videoObject)
					{
						return;
					}

					const auto nextVideo = *reinterpret_cast<const std::uintptr_t*>(videoObject + 0x08);

					const auto parameter1 = *reinterpret_cast<const std::uint32_t*>(videoObject + 0x0C);

					const auto parameter2 = *reinterpret_cast<const std::uint32_t*>(videoObject + 0x10);

					const bool isPlaceholder = parameter1 == 7 && parameter2 == 1;

					bool isV2PlayLogo = false;

					if (nextVideo)
					{
						const auto nextParameter1 = *reinterpret_cast<const std::uint32_t*>(nextVideo + 0x0C);

						const auto nextParameter2 = *reinterpret_cast<const std::uint32_t*>(nextVideo + 0x10);

						isV2PlayLogo = nextParameter1 == 7 && nextParameter2 == 1;
					}

					if (isV2PlayLogo || isPlaceholder)
					{
						ctx.ebx = (ctx.ebx & 0xFFFFFF00u) | 1u;
					}
				});
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV1 = 0.9424778819f;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;	

	SafetyHookMid m_resolutionWidth1Hook{};
	SafetyHookMid m_resolutionHeight1Hook{};
	SafetyHookMid m_resolution2Hook{};
	SafetyHookMid m_resolution3Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_skipIntroVideosHook{};

	enum ResolutionInstructionsIndices
	{
		Res1,
		Res2,
		Res3
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};	

	inline static RealMadridTheGameFix* s_instance_ = nullptr;
};

static std::unique_ptr<RealMadridTheGameFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<RealMadridTheGameFix>(hModule);
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