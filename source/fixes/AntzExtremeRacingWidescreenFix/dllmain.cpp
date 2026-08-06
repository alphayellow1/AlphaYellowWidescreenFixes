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
		return "1.5.2";
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
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{		
		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 81 7D ?? ?? ?? ?? ?? 0F 8C", "E9 ?? ?? ?? ?? 81 7D EC ?? ?? ?? ?? 0F 8C ?? ?? ?? ??",
		"75 ?? 8B 85 ?? ?? ?? ?? 83 E0", "83 EA ?? 89 15 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50", "66 A1 ?? ?? ?? ?? 66 A3");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ResListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ResListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 3: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ResListUnlock3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 4: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ResListUnlock4] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock1], 6);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock1] + 13, 6);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock1] + 26, 6);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock2] + 12, 6);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock2] + 25, 6);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock3], 2);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock3] + 13, 2);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock3] + 25, 2);
			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock3] + 37, 2);

			Memory::WriteNOPs(m_resolutionScansResult[ResListUnlock4], 3);

			m_resolutionWidthAddress = Memory::GetPointerFromAddress(m_resolutionScansResult[ResWidthHeight] + 2, Memory::PointerMode::Absolute);
			m_resolutionHeightAddress = Memory::GetPointerFromAddress(m_resolutionScansResult[ResWidthHeight] + 15, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(s_instance_->m_resolutionWidthAddress);
				uint32_t& iCurrentHeight = Memory::ReadMem(s_instance_->m_resolutionHeightAddress);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteMenuARAndFOV();
			});
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 40 ?? D8 49 ?? D9 5D ?? 8B 55", "68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D",
		"68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E8", "8B 8C 11 ?? ?? ?? ?? 89 48");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[MenuFOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[MenuFOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV1], 3);

			m_cameraHFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = Memory::ReadMem(ctx.eax + 0x3C);
				s_instance_->m_newCameraHFOV = Maths::CalculateNewFOV_MultiplierBased(fCurrentCameraHFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraHFOV);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV1] + 15, 3);

			m_cameraVFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[FOV1] + 15, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraVFOV = s_instance_->m_newCameraHFOV / m_oldAspectRatio;
				FPU::FLD(s_instance_->m_newCameraVFOV);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV3], 7);

			m_cameraFOV3Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV3 = Memory::ReadMem(ctx.ecx + ctx.edx + 0x00577AFC);
				s_instance_->m_newCameraFOV3 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV3, s_instance_->m_aspectRatioScale);
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV3);
			});
		}

		m_aspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B 0D",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 68", "C7 40 ?? ?? ?? ?? ?? 6A ?? 8B 4D ?? 8B 51 ?? 52 68",
		"C7 45 ?? ?? ?? ?? ?? E9 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B 45 ?? 6B C0",
		"C7 45 ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05");
		if (Memory::AreAllSignaturesValid(m_aspectRatioScansResult) == true)
		{
			spdlog::info("Menu Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScansResult[MenuAR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScansResult[MenuAR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is{:s} + {:x}", ExeName().c_str(), m_aspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScansResult[AR5] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_aspectRatioScansResult, AR3, AR5, 0, 7);

			m_aspectRatio3Hook = safetyhook::create_mid(m_aspectRatioScansResult[AR3], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio3 = 1.6f * s_instance_->m_aspectRatioScale;
				*reinterpret_cast<float*>(ctx.eax + 0x28) = s_instance_->m_newAspectRatio3;
			});

			m_aspectRatio4Hook = safetyhook::create_mid(m_aspectRatioScansResult[AR4], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio4 = 1.0f * s_instance_->m_aspectRatioScale;
				*reinterpret_cast<float*>(ctx.ebp - 0xC) = s_instance_->m_newAspectRatio4;
			});

			m_aspectRatio5Hook = safetyhook::create_mid(m_aspectRatioScansResult[AR5], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio5 = 1.0f * s_instance_->m_aspectRatioScale;
				*reinterpret_cast<float*>(ctx.ebp - 0xC) = s_instance_->m_newAspectRatio5;
			});
		}

		m_deviceInputFixScansResult = Memory::PatternScan(ExeModule(), "74 ?? 8B F4 68 ?? ?? ?? ?? 68", "3B F4 E8 ?? ?? ?? ?? 83 7D ?? ?? 74 ?? 8B F4",
		"74 ?? 8B F4 68 ?? ?? ?? ?? 6A ?? 8B 15", "83 7D ?? ?? 75 ?? B8 ?? ?? ?? ?? EB ?? 83 7D ?? ?? 75 ?? B8 ?? ?? ?? ?? EB ?? 83 7D",
		"83 7D ?? ?? 74 ?? E9 ?? ?? ?? ?? 8B F4 6A ?? FF 15");
		if (Memory::AreAllSignaturesValid(m_deviceInputFixScansResult) == true)
		{
			spdlog::info("Keyboard Recovery Branch: Address is {:s}+{:x}", ExeName().c_str(), m_deviceInputFixScansResult[KeyboardRecoveryBranch] - (std::uint8_t*)ExeModule());
			spdlog::info("Keyboard Get Device State Result: Address is {:s}+{:x}", ExeName().c_str(), m_deviceInputFixScansResult[KeyboardGetDeviceStateResult] - (std::uint8_t*)ExeModule());
			spdlog::info("Mouse Recovery Branch: Address is {:s}+{:x}", ExeName().c_str(), m_deviceInputFixScansResult[MouseRecoveryBranch] - (std::uint8_t*)ExeModule());
			spdlog::info("Standard Keyboard Hook: Address is {:s}+{:x}", ExeName().c_str(), m_deviceInputFixScansResult[StandardKeyboardHook] - (std::uint8_t*)ExeModule());
			spdlog::info("Low Level Keyboard Hook: Address is {:s}+{:x}", ExeName().c_str(), m_deviceInputFixScansResult[LowLevelKeyboardHook] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_deviceInputFixScansResult[KeyboardRecoveryBranch], 2);
			Memory::PatchBytes(m_deviceInputFixScansResult[KeyboardGetDeviceStateResult], "\x89\x45\xFC\x90\x90\x90\x90");
			Memory::WriteNOPs(m_deviceInputFixScansResult[MouseRecoveryBranch], 2);
			Memory::PatchBytes(m_deviceInputFixScansResult[StandardKeyboardHook], "\xEB\x25\x90\x90");
			Memory::PatchBytes(m_deviceInputFixScansResult[LowLevelKeyboardHook], "\xE9\xA0\x00\x00\x00\x90");
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 83 3D ?? ?? ?? ?? ?? 0F 8F");
			if (m_skipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Scan: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_skipIntroVideosScanResult, "\xE9\x74\x01\x00\x00\x90");
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos scan memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalMenuFOV = 0.5f;

	bool m_skipIntroVideos = false;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_aspectRatioScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::vector<std::uint8_t*> m_deviceInputFixScansResult{};
	std::uint8_t* m_skipIntroVideosScanResult = nullptr;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_aspectRatio5Hook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};
	SafetyHookMid m_cameraFOV3Hook{};

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

	enum DeviceInputFixSignatures
	{
		KeyboardRecoveryBranch,
		KeyboardGetDeviceStateResult,
		MouseRecoveryBranch,
		StandardKeyboardHook,
		LowLevelKeyboardHook,
		Count
	};

	void WriteMenuARAndFOV()
	{
		m_newMenuAspectRatio = m_newAspectRatio;
		m_newMenuFOV = m_originalMenuFOV * m_aspectRatioScale;

		Memory::Write(m_aspectRatioScansResult[MenuAR1] + 1, m_newMenuAspectRatio);
		Memory::Write(m_aspectRatioScansResult[MenuAR2] + 1, m_newMenuAspectRatio);
		Memory::Write(m_cameraFOVScansResult[MenuFOV1] + 1, m_newMenuFOV);
		Memory::Write(m_cameraFOVScansResult[MenuFOV2] + 1, m_newMenuFOV);
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