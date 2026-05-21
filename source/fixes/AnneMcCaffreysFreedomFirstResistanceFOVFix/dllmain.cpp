#include "..\..\common\FixBase.hpp"

class FreedomFirstResistanceFix final : public FixBase
{
public:
	explicit FreedomFirstResistanceFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~FreedomFirstResistanceFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AnneMcCaffreysFreedomFirstResistanceFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Anne McCaffrey's Freedom: First Resistance";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Freedom.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 5C 24 ?? 55 8B 6C 24 ?? 56 8B F1 8D 45");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->ResolutionMidHook(ctx);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 9E ?? ?? ?? ?? FF 50 ?? 8B 16 8B CE FF 52 ?? A1",
		"D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 9E ?? ?? ?? ?? FF 50 ?? 8B 16 8B CE FF 52 ?? 89 35", "D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 98",
		"D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 9A ?? ?? ?? ?? 8B 4E ?? 8B 01 FF 50 ?? 8B 4E ?? 8B 11 FF 52 ?? 8B 46 ?? 8B 0D ?? ?? ?? ?? 5E 89 88 ?? ?? ?? ?? 8B 4C 24 ?? 64 89 0D ?? ?? ?? ?? 83 C4 ?? C3 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 64 A1",
		"D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 9A ?? ?? ?? ?? 8B 4E ?? 8B 01 FF 50 ?? 8B 4E ?? 8B 11 FF 52 ?? 8B 46 ?? 8B 0D ?? ?? ?? ?? 5E 89 88 ?? ?? ?? ?? 8B 4C 24 ?? 64 89 0D ?? ?? ?? ?? 83 C4 ?? C3 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 6A",
		"D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 9A ?? ?? ?? ?? 8B 4E ?? 8B 01 FF 50 ?? 8B 4E ?? 8B 11 FF 52 ?? 8B 46 ?? 8B 0D ?? ?? ?? ?? 5E 89 88 ?? ?? ?? ?? 8B 4C 24 ?? 64 89 0D ?? ?? ?? ?? 83 C4 ?? C3 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 8A 0D",
		"D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 99", "DD 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? C7 86");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV7] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV8] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, FOV1, FOV8, 0, 12);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook(s_instance_->m_fovFactor);
			});

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook();
			});

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook();
			});

			m_cameraFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook();
			});

			m_cameraFOV5Hook = safetyhook::create_mid(CameraFOVScansResult[FOV5], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook();
			});

			m_cameraFOV6Hook = safetyhook::create_mid(CameraFOVScansResult[FOV6], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook();
			});

			m_cameraFOV7Hook = safetyhook::create_mid(CameraFOVScansResult[FOV7], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook();
			});

			m_cameraFOV8Hook = safetyhook::create_mid(CameraFOVScansResult[FOV8], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVInstructionsMidHook();
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 1.5707963268f;

	bool bResolutionReady = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};
	SafetyHookMid m_cameraFOV5Hook{};
	SafetyHookMid m_cameraFOV6Hook{};
	SafetyHookMid m_cameraFOV7Hook{};
	SafetyHookMid m_cameraFOV8Hook{};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7,
		FOV8
	};

	void ResolutionMidHook(SafetyHookContext& ctx)
	{
		int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x28);
		int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x2C);
		m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(m_originalCameraFOV, m_aspectRatioScale);

		bResolutionReady = true;
	}

	void CameraFOVInstructionsMidHook(float fovFactor = 1.0f)
	{
		if (bResolutionReady == false)
		{
			return;
		}

		FPU::FLD(m_newCameraFOV * fovFactor);
	}

	inline static FreedomFirstResistanceFix* s_instance_ = nullptr;
};

static std::unique_ptr<FreedomFirstResistanceFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<FreedomFirstResistanceFix>(hModule);
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