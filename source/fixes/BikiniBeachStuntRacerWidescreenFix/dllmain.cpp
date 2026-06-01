#include "..\..\common\FixBase.hpp"

class BikiniBeachStuntRacerFix final : public FixBase
{
public:
	explicit BikiniBeachStuntRacerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BikiniBeachStuntRacerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BikiniBeachStuntRacerWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Bikini Beach: Stunt Racer";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Bikini Beach Stunt Racer.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? 33 C0 8B 5C 24",
		"B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? B8 ?? ?? ?? ?? EB ?? 33 C0 3B C7",
		"68 ?? ?? ?? ?? EB ?? 68 ?? ?? ?? ?? EB ?? 68 ?? ?? ?? ?? EB ?? 68 ?? ?? ?? ?? EB ?? 68 ?? ?? ?? ?? EB ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? E8 ?? ?? ?? ?? 56");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[List1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[List2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[List3] - (std::uint8_t*)ExeModule());

			// Resolution List 1 - Widths
			Memory::Write(ResolutionScansResult[List1] + 1, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 8, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 15, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 22, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 29, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 36, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 43, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 50, m_newResX);
			Memory::Write(ResolutionScansResult[List1] + 57, m_newResX);
			// Resolution List 1 - Heights
			Memory::Write(ResolutionScansResult[List1] + 87, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 94, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 101, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 108, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 115, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 122, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 129, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 136, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 143, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 150, m_newResY);
			Memory::Write(ResolutionScansResult[List1] + 157, m_newResY);

			// Resolution List 2 - Widths
			Memory::Write(ResolutionScansResult[List2] + 1, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 8, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 15, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 22, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 29, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 36, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 43, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 50, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 57, m_newResX);
			Memory::Write(ResolutionScansResult[List2] + 64, m_newResX);
			// Resolution List 2 - Heights
			Memory::Write(ResolutionScansResult[List2] + 93, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 100, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 107, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 114, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 121, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 128, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 135, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 142, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 149, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 156, m_newResY);
			Memory::Write(ResolutionScansResult[List2] + 163, m_newResY);

			m_sNewResString = std::to_string(m_newResX) + " x " + std::to_string(m_newResY);
			m_cNewResString = m_sNewResString.c_str();

			// Resolution List 3 - Strings (640 x 480, 800 x 600, 1024 x 768, 1280 x 1024, 1600 x 1200)
			Memory::Write(ResolutionScansResult[List3] + 1, m_cNewResString);
			Memory::Write(ResolutionScansResult[List3] + 8, m_cNewResString);
			Memory::Write(ResolutionScansResult[List3] + 15, m_cNewResString);
			Memory::Write(ResolutionScansResult[List3] + 22, m_cNewResString);
			Memory::Write(ResolutionScansResult[List3] + 29, m_cNewResString);
		}

		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto CameraHFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "D9 43 ?? D8 23 D9 5C 24", "D9 43 ?? D8 03", "D9 83 ?? ?? ?? ?? 8B 93 ?? ?? ?? ?? D9 C0",
		"8B 8B ?? ?? ?? ?? 56 8D 54 24");
		if (Memory::AreAllSignaturesValid(CameraHFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraHFOVInstructionsScansResult[HFOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera HFOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraHFOVInstructionsScansResult[HFOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera HFOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraHFOVInstructionsScansResult[HFOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera HFOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraHFOVInstructionsScansResult[HFOV4] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV1], 5);

			m_cameraHFOV1Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraHFOVMidHook(ctx.ebx + 0x4, FLD);
				s_instance_->CameraHFOVMidHook(ctx.ebx, FSUB);
			});

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV2], 5);

			m_cameraHFOV2Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraHFOVMidHook(ctx.ebx + 0x4, FLD);
				s_instance_->CameraHFOVMidHook(ctx.ebx, FADD);
			});

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV3], 6);

			m_cameraHFOV3Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->s_instance_->CameraHFOVMidHook(ctx.ebx + 0x15C, FLD);
			});

			Memory::WriteNOPs(CameraHFOVInstructionsScansResult[HFOV4], 6);

			m_cameraHFOV4Hook = safetyhook::create_mid(CameraHFOVInstructionsScansResult[HFOV4], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV4 = Memory::ReadMem(ctx.ebx + 0x158);
				s_instance_->m_newCameraHFOV4 = fCurrentCameraHFOV4 * s_instance_->m_aspectRatioScale;
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraHFOV4);
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 8C 24 ?? ?? ?? ?? D9 1C 24");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 7);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x84);

				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;	

	SafetyHookMid m_cameraHFOV1Hook{};
	SafetyHookMid m_cameraHFOV2Hook{};
	SafetyHookMid m_cameraHFOV3Hook{};
	SafetyHookMid m_cameraHFOV4Hook{};
	SafetyHookMid m_cameraFOVHook{};

	float m_newCameraHFOV = 0.0f;
	float m_newCameraHFOV4 = 0.0f;
	std::string m_sNewResString = "";
	const char* m_cNewResString = "";

	enum ResolutionInstructionsIndex
	{
		List1,
		List2,
		List3
	};

	enum CameraHFOVInstructionsIndices
	{
		HFOV1,
		HFOV2,
		HFOV3,
		HFOV4
	};

	enum InstructionType
	{
		FLD,
		FSUB,
		FADD
	};

	void CameraHFOVMidHook(uintptr_t HFOVAddress, InstructionType DestinationInstruction)
	{
		float& fCurrentCameraHFOV = Memory::ReadMem(HFOVAddress);

		m_newCameraHFOV = fCurrentCameraHFOV * m_aspectRatioScale;

		switch (DestinationInstruction)
		{
			case FLD:
			{
				FPU::FLD(m_newCameraHFOV);
				break;
			}

			case FSUB:
			{
				FPU::FSUB(m_newCameraHFOV);
				break;
			}

			case FADD:
			{
				FPU::FADD(m_newCameraHFOV);
				break;
			}
		}
	}

	inline static BikiniBeachStuntRacerFix* s_instance_ = nullptr;
};

static std::unique_ptr<BikiniBeachStuntRacerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BikiniBeachStuntRacerFix>(hModule);
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