#include "..\..\common\FixBase.hpp"

class FilaWorldTourTennisFix final : public FixBase
{
public:
	explicit FilaWorldTourTennisFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~FilaWorldTourTennisFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "FilaWorldTourTennisFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "Fila World Tour Tennis";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "FWTT.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 0D");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.edx);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionScanResult + 16, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResY = Memory::ReadRegister(ctx.ecx);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteStaticARandFOVs();
				s_instance_->m_resolutionWidthHook.disable();
				s_instance_->m_resolutionHeightHook.disable();
			});
		}
		else
		{
			spdlog::info("Failed to locate resolution instructions scan memory address.");
			return;
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 8B 4E ?? 6A", "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 74 ?? 83 F8",
		"C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? DB 44 24", "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8D 44 24", "D8 0D ?? ?? ?? ?? DC C0 D8 25 ?? ?? ?? ?? D8 2D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24",
		"D8 25 ?? ?? ?? ?? D8 2D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24", "D8 0D ?? ?? ?? ?? D9 5C 24 ?? DB 44 24 ?? DA 35");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[GameplayCamAR] - (std::uint8_t*)ExeModule());
			spdlog::info("Character Selection Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CharSelectionAR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Character Selection Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CharSelectionAR2] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUDAR1] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUDAR2] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUDAR3] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUDAR4] - (std::uint8_t*)ExeModule());			

			Memory::WriteNOPs(AspectRatioScansResult, CharSelectionAR1, CharSelectionAR2, 0, 8);

			m_characterSelectionHFOV1Hook = safetyhook::create_mid(AspectRatioScansResult[CharSelectionAR1], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCharacterSelectionHFOV1 = s_instance_->m_originalCharacterSelectionAR1 * s_instance_->m_aspectRatioScale;

				*reinterpret_cast<float*>(ctx.esp + 0x54) = s_instance_->m_newCharacterSelectionHFOV1;
			});

			m_characterSelectionHFOV2Hook = safetyhook::create_mid(AspectRatioScansResult[CharSelectionAR2], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCharacterSelectionHFOV2 = s_instance_->m_originalCharacterSelectionAR2 * s_instance_->m_aspectRatioScale;

				*reinterpret_cast<float*>(ctx.esp + 0x54) = s_instance_->m_newCharacterSelectionHFOV2;
			});

			m_hudAspectRatioAddress = Memory::GetPointerFromAddress(AspectRatioScansResult[HUDAR1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(AspectRatioScansResult, HUDAR1, HUDAR4, 0, 6);

			m_hudAspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[HUDAR1], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(s_instance_->m_hudAspectRatioAddress, ECX, ctx);
			});

			m_hudAspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[HUDAR2], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(s_instance_->m_hudAspectRatioAddress, FMUL, ctx);
			});

			m_hudAspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[HUDAR3], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(s_instance_->m_hudAspectRatioAddress, FSUB, ctx);
			});

			m_hudAspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[HUDAR4], [](SafetyHookContext& ctx)
			{
				s_instance_->HUDAspectRatioMidHook(s_instance_->m_hudAspectRatioAddress, FMUL, ctx);
			});
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 8B CF E8 ?? ?? ?? ?? D9 5C 24", "68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 44 24",
		"68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 86", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CE", "D8 42 ?? 8B CE",
		"DB 40 ?? 51 8B CE D9 1C ?? E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 57");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay1] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay2] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay3] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay4] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes1] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, Cutscenes1, Cutscenes2, 0, 3);

			m_cutscenesFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCutscenesFOV1 = Memory::ReadMem(ctx.edx + 0x10);
				s_instance_->m_newCutscenesFOV1 = Maths::CalculateNewFOV_DegBased(fCurrentCutscenesFOV1, s_instance_->m_aspectRatioScale);
				FPU::FADD(s_instance_->m_newCutscenesFOV1);
			});

			m_cutscenesFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes2], [](SafetyHookContext& ctx)
			{
				int& iCurrentCameraFOV6 = Memory::ReadMem(ctx.eax + 0x14);
				s_instance_->m_newCutscenesFOV2 = (uint32_t)(Maths::CalculateNewFOV_DegBased((float)iCurrentCameraFOV6, s_instance_->m_aspectRatioScale));
				FPU::FILD(s_instance_->m_newCutscenesFOV2);
			});
		}

		if (m_runMultipleInstances == true)
		{
			auto MultipleInstancesScanResult = Memory::PatternScan(ExeModule(), "75 ?? 8A 54 24 ?? 8D 44 24 ?? 68 ?? ?? ?? ?? 50 88 54 24 ?? E8 ?? ?? ?? ?? 8B 84 24");
			if (MultipleInstancesScanResult)
			{
				spdlog::info("Multiple Instances Check Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), MultipleInstancesScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(MultipleInstancesScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check instructions scan memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV1 = 60.0f;
	static constexpr float m_originalCameraFOV2 = 25.0f;
	static constexpr float m_originalCharacterSelectionAR1 = 320.0f;
	static constexpr float m_originalCharacterSelectionAR2 = 224.0f;

	bool m_runMultipleInstances = false;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_characterSelectionHFOV1Hook{};
	SafetyHookMid m_characterSelectionHFOV2Hook{};
	SafetyHookMid m_hudAspectRatio1Hook{};
	SafetyHookMid m_hudAspectRatio2Hook{};
	SafetyHookMid m_hudAspectRatio3Hook{};
	SafetyHookMid m_hudAspectRatio4Hook{};
	SafetyHookMid m_cutscenesFOV1Hook{};
	SafetyHookMid m_cutscenesFOV2Hook{};

	std::vector<std::uint8_t*> AspectRatioScansResult;
	std::vector<std::uint8_t*> CameraFOVScansResult;

	float m_newCharacterSelectionHFOV1 = 0.0f;
	float m_newCharacterSelectionHFOV2 = 0.0f;
	uintptr_t m_hudAspectRatioAddress = 0;
	float m_newHUDAspectRatio = 0.0f;
	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;
	float m_newCutscenesFOV1 = 0.0f;
	uint32_t m_newCutscenesFOV2 = 0;

	enum AspectRatioInstructionsIndices
	{
		GameplayCamAR,
		CharSelectionAR1,
		CharSelectionAR2,
		HUDAR1,
		HUDAR2,
		HUDAR3,
		HUDAR4
	};

	enum FOVInstructionsIndices
	{
		Gameplay1,
		Gameplay2,
		Gameplay3,
		Gameplay4,
		Cutscenes1,
		Cutscenes2
	};

	enum DestinationInstructions
	{
		ECX,
		FMUL,
		FSUB
	};

	void WriteStaticARandFOVs()
	{
		m_newCameraFOV1 = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV1, m_aspectRatioScale) * m_fovFactor;
		m_newCameraFOV2 = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV2, m_aspectRatioScale) * m_fovFactor;

		Memory::Write(AspectRatioScansResult[GameplayCamAR] + 1, m_newAspectRatio);
		Memory::Write(CameraFOVScansResult, Gameplay1, Gameplay3, 1, m_newCameraFOV1);
		Memory::Write(CameraFOVScansResult[Gameplay4] + 1, m_newCameraFOV2);
	}

	void HUDAspectRatioMidHook(uintptr_t address, DestinationInstructions destinationInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentHUDAspectRatio = Memory::ReadMem(address);

		m_newHUDAspectRatio = fCurrentHUDAspectRatio * m_aspectRatioScale;

		switch (destinationInstruction)
		{
		case ECX:
			ctx.ecx = std::bit_cast<uintptr_t>(m_newHUDAspectRatio);
			break;

		case FMUL:
			FPU::FMUL(m_newHUDAspectRatio);
			break;

		case FSUB:
			FPU::FSUB(m_newHUDAspectRatio);
			break;
		}
	}

	inline static FilaWorldTourTennisFix* s_instance_ = nullptr;
};

static std::unique_ptr<FilaWorldTourTennisFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<FilaWorldTourTennisFix>(hModule);
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