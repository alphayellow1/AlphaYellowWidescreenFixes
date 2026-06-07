#include "..\..\common\FixBase.hpp"

class DickJohnsonV8ChallengeFix final : public FixBase
{
public:
	explicit DickJohnsonV8ChallengeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DickJohnsonV8ChallengeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DickJohnsonV8ChallengeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Dick Johnson V8 Challenge";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Dick Johnson V8 Challenge.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "A0 ?? ?? ?? ?? 84 C0 A1");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionWidthAddress = Memory::GetPointerFromAddress(ResolutionScanResult - 9, Memory::PointerMode::Absolute);
			m_resolutionHeightAddress = Memory::GetPointerFromAddress(ResolutionScanResult - 14, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(s_instance_->m_resolutionWidthAddress);
				uint32_t& iCurrentHeight = Memory::ReadMem(s_instance_->m_resolutionHeightAddress);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 8B 11 68", "D8 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 50",
		"D8 0D ?? ?? ?? ?? 8B 52 ?? 52 8B 15 ?? ?? ?? ?? 52 8B 94 24 ?? ?? ?? ?? 51 D9 1C ?? 52 8B 94 24 ?? ?? ?? ?? 52 FF 90 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 52 ?? 8B 01 8B 52 ?? 52 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 51 8B 40 ?? 8B 11 D9 40 ?? D8 0D ?? ?? ?? ?? D9 1C ?? FF 52 ?? 8B 0D");

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 84 24 ?? ?? ?? ?? 50 FF 92 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 8B 11",
		"8B 84 24 ?? ?? ?? ?? 50 FF 92 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 0D ?? ?? ?? ?? 6A",
		"8B 94 24 ?? ?? ?? ?? 52 FF 90 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 52 ?? 8B 01 8B 52 ?? 52 FF 50 ?? 8B 0D ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B 01 FF 50 ?? A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 51 8B 40 ?? 8B 11 D9 40 ?? D8 0D ?? ?? ?? ?? D9 1C ?? FF 52 ?? 8B 0D");

		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Pause Menu Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[PauseMenuAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayAR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayAR2] - (std::uint8_t*)ExeModule());			

			Memory::WriteNOPs(AspectRatioScansResult[PauseMenuAR], 6);

			m_pauseMenuAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[PauseMenuAR], AspectRatioMidHook);

			Memory::WriteNOPs(AspectRatioScansResult[GameplayAR1], 6);

			m_gameplayAspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[GameplayAR1], AspectRatioMidHook);

			Memory::WriteNOPs(AspectRatioScansResult[GameplayAR2], 6);

			m_gameplayAspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[GameplayAR2], AspectRatioMidHook);
		}

		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Pause Menu Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[PauseMenuFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GameplayFOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GameplayFOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[PauseMenuFOV], 7);

			m_pauseMenuFOVHook = safetyhook::create_mid(CameraFOVScansResult[PauseMenuFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0xB8, ctx.eax);
			});

			Memory::WriteNOPs(CameraFOVScansResult[GameplayFOV1], 7);

			m_gameplayFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[GameplayFOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x208, ctx.eax, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[GameplayFOV2], 7);

			m_gameplayFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[GameplayFOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x208, ctx.edx, s_instance_->m_fovFactor);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	uintptr_t m_resolutionWidthAddress = 0;
	uintptr_t m_resolutionHeightAddress = 0;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_pauseMenuAspectRatioHook{};
	SafetyHookMid m_gameplayAspectRatio1Hook{};
	SafetyHookMid m_gameplayAspectRatio2Hook{};
	SafetyHookMid m_pauseMenuFOVHook{};
	SafetyHookMid m_gameplayFOV1Hook{};
	SafetyHookMid m_gameplayFOV2Hook{};

	enum AspectRatioInstructionsIndices
	{
		PauseMenuAR,
		GameplayAR1,
		GameplayAR2
	};

	enum CameraFOVInstructionsIndices
	{
		PauseMenuFOV,
		GameplayFOV1,
		GameplayFOV2
	};	

	static void AspectRatioMidHook(SafetyHookContext& ctx)
	{
		s_instance_->m_newAspectRatio2 = 0.75f / s_instance_->m_aspectRatioScale;

		FPU::FMUL(s_instance_->m_newAspectRatio2);
	}

	void CameraFOVMidHook(uintptr_t FOVAddress, uintptr_t& DestInstruction, float fovFactor = 1.0f)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = (fCurrentCameraFOV / m_aspectRatioScale) / fovFactor;

		DestInstruction = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static DickJohnsonV8ChallengeFix* s_instance_ = nullptr;
};

static std::unique_ptr<DickJohnsonV8ChallengeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<DickJohnsonV8ChallengeFix>(hModule);
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