#include "..\..\common\FixBase.hpp"

class AntzFix final : public FixBase
{
public:
	explicit AntzFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AntzFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AntzExtremeRacingWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "Antz Extreme Racing";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "antzextremeracing.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 81 7D ?? ?? ?? ?? ?? 0F 8C", "E9 ?? ?? ?? ?? 81 7D EC ?? ?? ?? ?? 0F 8C ?? ?? ?? ??",
		"75 ?? 8B 85 ?? ?? ?? ?? 83 E0", "83 EA ?? 89 15 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50", "66 A1 ?? ?? ?? ?? 66 A3");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 3: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 4: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock4] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1], 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1] + 13, 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1] + 26, 6);

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock2] + 12, 6);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock2] + 25, 6);

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock3], 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock3] + 13, 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock3] + 25, 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock3] + 37, 2);

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock4], 3);

			m_resolutionWidthAddress = Memory::GetPointerFromAddress(ResolutionScansResult[ResWidthHeight] + 2, Memory::PointerMode::Absolute);
			m_resolutionHeightAddress = Memory::GetPointerFromAddress(ResolutionScansResult[ResWidthHeight] + 15, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(s_instance_->m_resolutionWidthAddress);
				uint32_t& iCurrentHeight = Memory::ReadMem(s_instance_->m_resolutionHeightAddress);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteMenuARAndFOV();
			});
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 40 ?? D8 49 ?? D9 5D ?? 8B 55", "68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D",
		"68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E8", "8B 8C 11 ?? ?? ?? ?? 89 48");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[MenuFOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[MenuFOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 3);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = Memory::ReadMem(ctx.eax + 0x3C);
				s_instance_->m_newCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV1] + 15, 3);

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScansResult[FOV1] + 15, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraVFOV = s_instance_->m_newCameraHFOV / m_oldAspectRatio;
				FPU::FLD(s_instance_->m_newCameraVFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV3], 7);

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = Memory::ReadMem(ctx.ecx + ctx.edx + 0x00577AFC);
				s_instance_->m_newCameraFOV3 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV3, s_instance_->m_aspectRatioScale);
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV3);
			});
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 68", "C7 40 ?? ?? ?? ?? ?? 6A ?? 8B 4D ?? 8B 51 ?? 52 68",
		"C7 45 ?? ?? ?? ?? ?? E9 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B 45 ?? 6B C0",
		"C7 45 ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Menu Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[MenuAR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[MenuAR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is{:s} + {:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR5] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult, AR3, AR5, 0, 7);

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio3 = 1.6f * s_instance_->m_aspectRatioScale;

				*reinterpret_cast<float*>(ctx.eax + 0x28) = s_instance_->m_newAspectRatio3;
			});

			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio4 = 1.0f * s_instance_->m_aspectRatioScale;

				*reinterpret_cast<float*>(ctx.ebp - 0xC) = s_instance_->m_newAspectRatio4;
			});

			m_aspectRatio5Hook = safetyhook::create_mid(AspectRatioScansResult[AR5], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio5 = 1.0f * s_instance_->m_aspectRatioScale;

				*reinterpret_cast<float*>(ctx.ebp - 0xC) = s_instance_->m_newAspectRatio5;
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalMenuFOV = 0.5f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_aspectRatio5Hook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};
	SafetyHookMid m_cameraFOV3Hook{};

	std::vector<std::uint8_t*> CameraFOVScansResult;
	std::vector<std::uint8_t*> AspectRatioScansResult;

	uintptr_t m_resolutionWidthAddress = 0;
	uintptr_t m_resolutionHeightAddress = 0;

	float m_newMenuAspectRatio = 0.0f;
	float m_newAspectRatio3 = 0.0f;
	float m_newAspectRatio4 = 0.0f;
	float m_newAspectRatio5 = 0.0f;
	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;
	float m_newMenuFOV = 0.0f;
	float m_newCameraFOV3 = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ResListUnlock1,
		ResListUnlock2,
		ResListUnlock3,
		ResListUnlock4,
		ResWidthHeight
	};
	
	enum CameraFOVInstructionsIndex
	{
		FOV1,
		MenuFOV1,
		MenuFOV2,
		FOV3
	};

	enum AspectRatioInstructionsIndices
	{
		MenuAR1,
		MenuAR2,
		AR3,
		AR4,
		AR5
	};

	void WriteMenuARAndFOV()
	{
		m_newMenuAspectRatio = m_newAspectRatio;
		m_newMenuFOV = m_originalMenuFOV * m_aspectRatioScale;

		Memory::Write(AspectRatioScansResult[MenuAR1] + 1, m_newMenuAspectRatio);
		Memory::Write(AspectRatioScansResult[MenuAR2] + 1, m_newMenuAspectRatio);
		Memory::Write(CameraFOVScansResult[MenuFOV1] + 1, m_newMenuFOV);
		Memory::Write(CameraFOVScansResult[MenuFOV2] + 1, m_newMenuFOV);
	}

	inline static AntzFix* s_instance_ = nullptr;
};

static std::unique_ptr<AntzFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AntzFix>(hModule);
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