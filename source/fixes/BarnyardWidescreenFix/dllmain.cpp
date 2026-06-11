#include "..\..\common\FixBase.hpp"

class BarnyardFix final : public FixBase
{
public:
	explicit BarnyardFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BarnyardFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BarnyardWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Barnyard";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Barnyard.exe");
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
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

        auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "C7 04 24 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24",
		"C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C6 46 ?? ?? C6 46", "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 8B 4C 24 ?? 8B 54 24 ?? 8B 44 24 ?? 89 4C 24",
		"C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C6 44 24 ?? ?? 74");
        if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
        {
            spdlog::info("Resolution 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
            spdlog::info("Resolution 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res2] - (std::uint8_t*)ExeModule());
            spdlog::info("Resolution 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res3] - (std::uint8_t*)ExeModule());
            spdlog::info("Resolution 4 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Res4] - (std::uint8_t*)ExeModule());

            Memory::Write(ResolutionScansResult[Res1] + 11, m_newResX);
            Memory::Write(ResolutionScansResult[Res1] + 3, m_newResY);
            Memory::Write(ResolutionScansResult[Res1] + 51, m_newResX);
            Memory::Write(ResolutionScansResult[Res1] + 59, m_newResY);
            Memory::Write(ResolutionScansResult[Res2] + 3, m_newResX);
            Memory::Write(ResolutionScansResult[Res2] + 10, m_newResY);
            Memory::Write(ResolutionScansResult[Res3] + 4, m_newResX);
            Memory::Write(ResolutionScansResult[Res3] + 12, m_newResY);
            Memory::Write(ResolutionScansResult[Res4] + 4, m_newResX);
            Memory::Write(ResolutionScansResult[Res4] + 12, m_newResY);
        }

        auto HUDWidthScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? FF 52 ?? 8B 4E");
        if (HUDWidthScanResult)
        {
            spdlog::info("HUD Width Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDWidthScanResult - (std::uint8_t*)ExeModule());

            m_newHUDWidth = 800.0f * m_aspectRatioScale;

            Memory::Write(HUDWidthScanResult + 1, m_newHUDWidth);
        }
        else
        {
            spdlog::error("Failed to locate HUD width instruction memory address.");
            return;
        }

        auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 53 ?? 52 8B CE E8 ?? ?? ?? ?? DD D8", "D9 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC 56 8B F1",
		"68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? DD D8 68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? DD D8 8B 44 24");
        if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
        {
            spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
            spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
            spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

            Memory::WriteNOPs(CameraFOVScansResult[FOV1], 3);

            m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
            {
				float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.ebx + 0x44);
				s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
				ctx.edx = std::bit_cast<uint32_t>(s_instance_->m_newCameraFOV1);
            });

            m_cameraFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[FOV2] + 2, Memory::PointerMode::Absolute);

            Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
            {
				float& fCurrentCameraFOV2 = Memory::ReadMem(s_instance_->m_cameraFOVAddress);
				s_instance_->m_newCameraFOV2 = fCurrentCameraFOV2 * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV2);
            });

            m_newCameraFOV3 = Maths::CalculateNewFOV_RadBased(1.04719758f, m_aspectRatioScale);

            Memory::Write(CameraFOVScansResult[FOV3] + 1, m_newCameraFOV3);
        }
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	float m_newHUDWidth = 0.0f;
	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;
	float m_newCameraFOV3 = 0.0f;
	uintptr_t m_cameraFOVAddress = 0;

	enum ResolutionInstructionsIndex
	{
		Res1,
		Res2,
		Res3,
		Res4
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3
	};

	inline static BarnyardFix* s_instance_ = nullptr;
};

static std::unique_ptr<BarnyardFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BarnyardFix>(hModule);
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