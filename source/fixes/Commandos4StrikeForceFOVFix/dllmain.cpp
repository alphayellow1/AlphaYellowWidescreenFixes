#include "..\..\common\FixBase.hpp"

class CommandosStrikeForceFix final : public FixBase
{
public:
	explicit CommandosStrikeForceFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~CommandosStrikeForceFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Commandos4StrikeForceFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Commandos 4: Strike Force";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "CommXPC.exe");
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
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 0A 89 0D ?? ?? ?? ?? 8B 4A");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.edx);
				s_instance_->m_newResY = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::info("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D9 5D ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC CC",
		"D8 0D ?? ?? ?? ?? D9 5D ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C2 ?? ?? CC CC CC CC CC CC CC 55");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());			

			Memory::WriteNOPs(AspectRatioScansResult, AR1, AR2, 0, 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], [](SafetyHookContext& ctx)
			{
				s_instance_->SetAR();
			});

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], [](SafetyHookContext& ctx)
			{
				s_instance_->SetAR();
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 45 ?? 50 E8 ?? ?? ?? ?? D9 51", "8B 45 ?? 8B 55 ?? 89 41 ?? 89 51 ?? 5D C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC 8B 41");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 3);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.ebp + 0x8);

				if (fCurrentCameraFOV1 != s_instance_->m_newCameraFOV1)
				{
					if (fCurrentCameraFOV1 == 1.221730471f)
					{
						s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
					}
				}

				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV1);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 3);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.ebp + 0x8);

				if (fCurrentCameraFOV2 != s_instance_->m_newCameraFOV2)
				{
					if (fCurrentCameraFOV2 == 1.221730471f)
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale);
					}
				}

				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV2);
			});
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "0F 83 ?? ?? ?? ?? 8B C7 C7 45");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScanResult, "\xE9\x87\x00\x00\x00\x90");
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos check instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_skipIntroVideos = false;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2
	};

	enum FOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	void SetAR()
	{
		m_newAspectRatio2 = 0.75f / m_aspectRatioScale;
		FPU::FMUL(m_newAspectRatio2);
	}

	inline static CommandosStrikeForceFix* s_instance_ = nullptr;
};

static std::unique_ptr<CommandosStrikeForceFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<CommandosStrikeForceFix>(hModule);
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