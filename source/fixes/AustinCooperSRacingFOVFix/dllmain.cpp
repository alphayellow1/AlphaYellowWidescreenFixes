#include "..\..\common\FixBase.hpp"

class AustinCooperSRacingFix final : public FixBase
{
public:
	explicit AustinCooperSRacingFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AustinCooperSRacingFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AustinCooperSRacingFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Austin Cooper S Racing";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AustinCooperS.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 02 A3 ?? ?? ?? ?? 8B 42");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.edx);
				s_instance_->m_newResY = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? 8B 46 ?? D9 5C 24 ?? D9 05", "D9 05 ?? ?? ?? ?? 89 6C 24");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] + 13 - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[AR1] + 13, 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1] + 13, [](SafetyHookContext& ctx)
			{
				FPU::FLD((float)s_instance_->m_newResX);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR1], 6);

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], [](SafetyHookContext& ctx)
			{
				FPU::FLD((float)s_instance_->m_newResY);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR2], 6);

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio2 = 1.0f / s_instance_->m_newAspectRatio;
				FPU::FLD(s_instance_->m_newAspectRatio2);
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 46 ?? 8B 4E ?? D9 1C 24");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 3);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.esi + 0x14);
				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_cameraFOVHook{};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2
	};

	inline static AustinCooperSRacingFix* s_instance_ = nullptr;
};

static std::unique_ptr<AustinCooperSRacingFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AustinCooperSRacingFix>(hModule);
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