#include "..\..\common\FixBase.hpp"

class AdrenalinExtremeShowFix final : public FixBase
{
public:
	explicit AdrenalinExtremeShowFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AdrenalinExtremeShowFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AdrenalinExtremeShowFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.6";
	}

	const char* TargetName() const override
	{
		return "Adrenalin: Extreme Show";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Adrenalin.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionInstructionsScanResult = Memory::PatternScan(ExeModule(), "A3 ?? ?? ?? ?? 2B 55");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)ExeModule());

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadRegister(ctx.eax);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionInstructionsScanResult + 11, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHeight = Memory::ReadRegister(ctx.edx);

				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);

				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to find resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioInstructionsScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D8 F9 D9 1C ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE F9 D9 5C 24 ?? 89 6C 24",
		"D9 05 ?? ?? ?? ?? D8 F9 D9 1C ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE F9 D9 5C 24 ?? 89 44 24", "D8 05 ?? ?? ?? ?? DB 44 24 ?? DE C9 D9 5C 24 ?? D9 44 24 ?? DB 5C 24 ?? 8B 44 24",
		"D9 05 ?? ?? ?? ?? D8 F9 D9 1C ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE F9 D9 5C 24 ?? 89 54 24");
		if (Memory::AreAllSignaturesValid(AspectRatioInstructionsScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioInstructionsScansResult[AR1] - (std::uint8_t*)ExeModule());

			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioInstructionsScansResult[AR2] - (std::uint8_t*)ExeModule());

			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioInstructionsScansResult[AR3] - (std::uint8_t*)ExeModule());

			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioInstructionsScansResult[AR4] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioInstructionsScansResult, AR1, AR4, 0, 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR1], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioInstructionsMidHook(FLD);
			});

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR2], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioInstructionsMidHook(FLD);
			});

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR3], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioInstructionsMidHook(FADD);
			});

			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioInstructionsScansResult[AR4], [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioInstructionsMidHook(FLD);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 87 ?? ?? ?? ?? D9 87 ?? ?? ?? ?? D8 E1 DE CA DE C1 D9 15 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? DE F1 D9 1C ?? E8 ?? ?? ?? ?? 59 D9 1D ?? ?? ?? ?? C7 87",
		"8B 81 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 59 C3 56", "8B 87 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B7 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 83 C4 ?? 5F C3 83 C4",
		"8B 83 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B3 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? C7 83", "D9 87 ?? ?? ?? ?? D9 87 ?? ?? ?? ?? D8 E1 DE CA DE C1 D9 15 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? DE F1 D9 1C ?? E8 ?? ?? ?? ?? 59 D9 1D ?? ?? ?? ?? 81 C4",
		"8B 81 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 59 C3 53 83 EC", "8B 85 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05", "8B 85 ?? ?? ?? ?? 57 A3", "8B 87 ?? ?? ?? ?? A3",
		"8B 81 ?? ?? ?? ?? A3 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D8 B1 ?? ?? ?? ?? D9 1C ?? E8 ?? ?? ?? ?? D9 1D ?? ?? ?? ?? 59 C3 53 81 EC ?? ?? ?? ?? 8B D9 8B 4B ?? 85 C9 0F 84 ?? ?? ?? ?? 8B 01 8D 54 24",
		"D9 87 ?? ?? ?? ?? D8 E1 DE CA DE C1 D9 15 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? DE F1 D9 1C 24 E8 ?? ?? ?? ?? 59 D9 1D ?? ?? ?? ?? 81 C4");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			// These instructions are located in the following functions: ChaseCamera3::switch_to, ChaseCamera3::act, FixedReplayCamera::switch_to, ChaseCamera2::switch_to, AnimatedCamera::switch_to, FPSJeepCamera::switch_to, CMcarctrl::act
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV7] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV8] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV9] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV10] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 11: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV11] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, FOV1, FOV11, 0, 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edi + 0x90, FLD, ctx);
			});

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ecx + 0x90, EAX, ctx);
			});

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edi + 0x90, EAX, ctx);
			});

			m_cameraFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebx + 0x120, EAX, ctx);
			});

			m_cameraFOV5Hook = safetyhook::create_mid(CameraFOVScansResult[FOV5], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edi + 0x90, FLD, ctx);
			});

			m_cameraFOV6Hook = safetyhook::create_mid(CameraFOVScansResult[FOV6], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ecx + 0x90, EAX, ctx);
			});

			m_cameraFOV7Hook = safetyhook::create_mid(CameraFOVScansResult[FOV7], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebp + 0x90, EAX, ctx);
			});

			m_cameraFOV8Hook = safetyhook::create_mid(CameraFOVScansResult[FOV8], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebp - 0xAC, EAX, ctx);
			});

			m_cameraFOV9Hook = safetyhook::create_mid(CameraFOVScansResult[FOV9], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edi + 0x90, EAX, ctx);
			});

			m_cameraFOV10Hook = safetyhook::create_mid(CameraFOVScansResult[FOV10], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ecx + 0x90, EAX, ctx);
			});

			m_cameraFOV11Hook = safetyhook::create_mid(CameraFOVScansResult[FOV11], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.edi + 0xC0, FLD, ctx);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;
	float m_newAspectRatio2 = 1.0f;

	uint32_t m_currentWidth;
	uint32_t m_currentHeight;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};
	SafetyHookMid m_cameraFOV5Hook{};
	SafetyHookMid m_cameraFOV6Hook{};
	SafetyHookMid m_cameraFOV7Hook{};
	SafetyHookMid m_cameraFOV8Hook{};
	SafetyHookMid m_cameraFOV9Hook{};
	SafetyHookMid m_cameraFOV10Hook{};
	SafetyHookMid m_cameraFOV11Hook{};

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2,
		AR3,
		AR4
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7,
		FOV8,
		FOV9,
		FOV10,
		FOV11,
	};

	enum DestInstruction
	{
		FLD,
		FADD,
		EAX
	};

	void AspectRatioInstructionsMidHook(DestInstruction destInstruction)
	{
		m_newAspectRatio2 = m_aspectRatioScale;

		switch (destInstruction)
		{
			case FLD:
			{
				FPU::FLD(m_newAspectRatio2);
				break;
			}

			case FADD:
			{
				FPU::FADD(m_newAspectRatio2);
				break;
			}
		}
	}

	void CameraFOVMidHook(uintptr_t SourceAddress, DestInstruction destInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(SourceAddress);

		m_newCameraFOV = fCurrentCameraFOV / m_fovFactor;

		switch (destInstruction)
		{
			case FLD:
			{
				FPU::FLD(m_newCameraFOV);
				break;
			}

			case EAX:
			{
				ctx.eax = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}
		}
	}

	inline static AdrenalinExtremeShowFix* s_instance_ = nullptr;
};

static std::unique_ptr<AdrenalinExtremeShowFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AdrenalinExtremeShowFix>(hModule);
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