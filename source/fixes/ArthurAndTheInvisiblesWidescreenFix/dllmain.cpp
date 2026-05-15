#include "..\..\common\FixBase.hpp"

class ArthurAndTheInvisiblesFix final : public FixBase
{
public:
	explicit ArthurAndTheInvisiblesFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ArthurAndTheInvisiblesFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ArthurAndTheInvisiblesWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Arthur and the Invisibles";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "GameModule.elb");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "75 ?? 81 7C 24 ?? ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 7F ?? 46",
		"75 ?? 81 7C 24 ?? ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 7F ?? 4E", "8B 0A 89 0D ?? ?? ?? ?? 8B 6A");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1], 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1] + 10, 2);
			Memory::PatchBytes(ResolutionScansResult[ResListUnlock1] + 20, "\xEB\x0E");
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock2], 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock2] + 10, 2);
			Memory::PatchBytes(ResolutionScansResult[ResListUnlock2] + 20, "\xEB\x0B");
			
			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_newAspectRatio2 = 0.75f / s_instance_->m_aspectRatioScale;
			});			
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "8B 15 ?? ?? ?? ?? 89 51 ?? EB", "D9 05 ?? ?? ?? ?? 8B 48 ?? 8D 44 24", "D8 0D ?? ?? ?? ?? 33 C0 66 8B 86");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CameraAR] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUD_AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUD_AR2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult, CameraAR, HUD_AR2, 0, 6);

			m_cameraAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[CameraAR], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio2);
			});

			m_hudAspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[HUD_AR1], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio2);
			});

			m_hudAspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[HUD_AR2], [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newAspectRatio2);
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 41 ?? D8 0D");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 4);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
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

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraAspectRatioHook{};
	SafetyHookMid m_hudAspectRatio1Hook{};
	SafetyHookMid m_hudAspectRatio2Hook{};
	SafetyHookMid m_cameraFOVHook{};

	enum ResolutionInstructionsIndex
	{
		ResListUnlock1,
		ResListUnlock2,
		ResWidthHeight
	};

	enum AspectRatioInstructionsIndex
	{
		CameraAR,
		HUD_AR1,
		HUD_AR2
	};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x14);

		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;

		FPU::FLD(m_newCameraFOV);
	}

	inline static ArthurAndTheInvisiblesFix* s_instance_ = nullptr;
};

static std::unique_ptr<ArthurAndTheInvisiblesFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ArthurAndTheInvisiblesFix>(hModule);
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