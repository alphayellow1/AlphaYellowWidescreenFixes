#include "..\..\common\FixBase.hpp"

class PolarExpressFix final : public FixBase
{
public:
	explicit PolarExpressFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PolarExpressFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ThePolarExpressWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "The Polar Express";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Config.exe") ||
		Util::stringcmp_caseless(exeName, "PolarExpress.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "Config.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "0F 8B ?? ?? ?? ?? 8B 55");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 6);
				Memory::WriteNOPs(ResolutionListUnlockScanResult + 28, 6);
				Memory::WriteNOPs(ResolutionListUnlockScanResult + 45, 2);
				Memory::PatchBytes(ResolutionListUnlockScanResult + 47, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate resolution list unlock scan memory address");
				return;
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "PolarExpress.exe"))
		{
			auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 08 89 0D ?? ?? ?? ?? 8B 50");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->m_newResX = Memory::ReadMem(ctx.eax);
					s_instance_->m_newResY = Memory::ReadMem(ctx.eax + 0x4);
					s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->m_resolutionHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}

			auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "8B 50 ?? 8B 4C 24 ?? 83 C0 ?? 89 11 8B 40",
			"D8 0D ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 83 C4 ?? 5E", "89 51 ?? 8B 8E ?? ?? ?? ?? 50");
			if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
			{
				spdlog::info("Cutscenes Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CutscenesAR] - (std::uint8_t*)ExeModule());
				spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayAR] - (std::uint8_t*)ExeModule());
				spdlog::info("Pause Menu Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[PauseMenuAR] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(AspectRatioScansResult[CutscenesAR], 3);

				m_cutscenesAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[CutscenesAR], [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentCutscenesAspectRatio = Memory::ReadMem(ctx.eax + 0x68);
					s_instance_->m_newCutscenesAspectRatio = s_instance_->m_currentCutscenesAspectRatio * s_instance_->m_aspectRatioScale;
					ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newCutscenesAspectRatio);
				});

				Memory::WriteNOPs(AspectRatioScansResult[GameplayAR], 6);

				m_gameplayAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[GameplayAR], [](SafetyHookContext& ctx)
				{
					s_instance_->m_newGameplayAspectRatio = 0.75f / s_instance_->m_aspectRatioScale;
					FPU::FMUL(s_instance_->m_newGameplayAspectRatio);
				});

				m_pauseMenuAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[PauseMenuAR], [](SafetyHookContext& ctx)
				{
					*reinterpret_cast<float*>(ctx.eax) = *reinterpret_cast<float*>(ctx.eax) * s_instance_->m_aspectRatioScale;
				});
			}

			auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 8E");
			if (CameraFOVScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVScanResult, 4);

				m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x10);
					s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					FPU::FLD(s_instance_->m_newCameraFOV);
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction scan memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_currentCutscenesAspectRatio = 0.0f;
	float m_newCutscenesAspectRatio = 0.0f;
	float m_newGameplayAspectRatio = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cutscenesAspectRatioHook{};
	SafetyHookMid m_gameplayAspectRatioHook{};
	SafetyHookMid m_pauseMenuAspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	enum ResolutionInstructionsIndex
	{
		ListUnlock1,
		ListUnlock2
	};

	enum AspectRatioInstructionsIndices
	{
		CutscenesAR,
		GameplayAR,
		PauseMenuAR
	};

	inline static PolarExpressFix* s_instance_ = nullptr;
};

static std::unique_ptr<PolarExpressFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<PolarExpressFix>(hModule);
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