#include "..\..\common\FixBase.hpp"

class TennisMastersSeries2003Fix final : public FixBase
{
public:
	explicit TennisMastersSeries2003Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TennisMastersSeries2003Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TennisMastersSeries2003FOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Tennis Masters Series 2003";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Tennis Masters Series 2003.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 51 ?? 89 50 ?? 8B 51 ?? 89 50 ?? 8B 51 ?? 89 50 ?? 8B 51 ?? 89 50 ?? 8A 51 ?? 88 50 ?? 8A 51 ?? 88 50 ?? 8B 51");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.ecx + 0xC);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.ecx + 0x10);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 8E ?? ?? ?? ?? 8B", "D9 84 8E ?? ?? ?? ?? D8 0D",
		"D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? D8", "D9 86 ?? ?? ?? ?? D8 A6 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? DE C9",
		"D9 84 86 C0 00 00 00 D8 0D ?? ?? ?? ?? 89 4C 24 24 8B 4E 14 89 54 24 28 8D 54 24 14 D8 0D ?? ?? ?? ?? 52 D9 F2");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera 1 to 4 FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay1_4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera 5 & 6 FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay5_6] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera 7 FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay7] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes1] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay1_4], 6);

			m_gameplayFOV1To4Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay1_4], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0xAC, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay5_6], 7);

			m_gameplayFOV5And6Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay5_6], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + ctx.ecx * 0x4 + 0xC0, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay7], 6);

			m_gameplayFOV7Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay7], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0xAC, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes1], 6);

			m_cutscenesFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0x9C);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes2], 7);

			m_cutscenesFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + ctx.eax * 0x4 + 0xC0);
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D8 70 ?? 8B 86", "D8 70 ?? D9 C1", "D8 71 ?? 8B 8E",
		"D8 B6 ?? ?? ?? ?? D9 C1 D9 E0 D9 5C 24 ?? D9 C1 D9 5C 24 ?? DE C9 D9 54 24 ?? D9 E0 D9 5C 24 ?? 8B 11");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult, AR1, AR3, 0, 3);
			Memory::WriteNOPs(AspectRatioScansResult[AR4], 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], AspectRatioMidHook);
			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], AspectRatioMidHook);
			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], AspectRatioMidHook);
			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], AspectRatioMidHook);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_gameplayFOV1To4Hook{};
	SafetyHookMid m_gameplayFOV5And6Hook{};
	SafetyHookMid m_gameplayFOV7Hook{};
	SafetyHookMid m_cutscenesFOV1Hook{};
	SafetyHookMid m_cutscenesFOV2Hook{};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3,
		AR4
	};

	enum CameraFOVInstructionsIndex
	{
		Gameplay1_4,
		Gameplay5_6,
		Gameplay7,
		Cutscenes1,
		Cutscenes2
	};

	static void AspectRatioMidHook(SafetyHookContext& ctx)
	{
		FPU::FDIV(s_instance_->m_newAspectRatio);
	}

	void CameraFOVMidHook(uintptr_t CameraFOVAddress, float fovFactor = 1.0f)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(CameraFOVAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;

		FPU::FLD(m_newCameraFOV);
	}

	inline static TennisMastersSeries2003Fix* s_instance_ = nullptr;
};

static std::unique_ptr<TennisMastersSeries2003Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<TennisMastersSeries2003Fix>(hModule);
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