#include "..\..\common\FixBase.hpp"

class BallisticsFix final : public FixBase
{
public:
	explicit BallisticsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BallisticsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BallisticsFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Ballistics";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Ballistics.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 6E ?? 33 DB");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x30);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x34);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 8A ?? ?? ?? ?? D9 5C 24 ?? D9 82");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "DC 3D ?? ?? ?? ?? D9 54 24", "D9 86 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? D8 0D");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.edx + 0x13C);

				if (fCurrentCameraFOV == 1.989700675f)
				{
					s_instance_->m_newCameraFOV1 = (1.0 / (double)s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newCameraFOV1 = 1.0 / (double)s_instance_->m_aspectRatioScale;
				}

				FPU::FDIVR(s_instance_->m_newCameraFOV1);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

			m_newCameraFOV2 = 3.1381019951f;

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newCameraFOV2);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	double m_fovFactor = 0.0;
	double m_newCameraFOV1 = 0.0;
	float m_newCameraFOV2 = 0.0f;

	inline static BallisticsFix* s_instance_ = nullptr;
};

static std::unique_ptr<BallisticsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BallisticsFix>(hModule);
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