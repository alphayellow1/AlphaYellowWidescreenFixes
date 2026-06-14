#include "..\..\common\FixBase.hpp"

class PetRacerFix final : public FixBase
{
public:
	explicit PetRacerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PetRacerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "PetRacerWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Pet Racer";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "PetRacer.exe");
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

		m_chromeEngineDllModule = Memory::GetHandle("ChromeEngine.dll");
		m_chromeEngineDllModuleName = Memory::GetModuleName(m_chromeEngineDllModule);

		auto ResolutionInstructionsScanResult = Memory::PatternScan(m_chromeEngineDllModule, "89 41 ?? 89 51 ?? C2 ?? ?? 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 8B 54 24");
		if (ResolutionInstructionsScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_chromeEngineDllModuleName.c_str(), ResolutionInstructionsScanResult - (std::uint8_t*)m_chromeEngineDllModule);

			Memory::WriteNOPs(ResolutionInstructionsScanResult, 6);

			m_resolutionHook = safetyhook::create_mid(ResolutionInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0xC) = s_instance_->m_newResX;
				*reinterpret_cast<int*>(ctx.ecx + 0x10) = s_instance_->m_newResY;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(m_chromeEngineDllModule, "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8A 81", "D8 4C 24 ?? D9 5C 24 ?? D8 C9");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", m_chromeEngineDllModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)m_chromeEngineDllModule);
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", m_chromeEngineDllModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)m_chromeEngineDllModule);
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", m_chromeEngineDllModuleName.c_str(), CameraFOVScansResult[FOV2] + 10 - (std::uint8_t*)m_chromeEngineDllModule);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCameraFOV = Memory::ReadMem(ctx.esp + 0x4);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 4); // NOP out the original instruction

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = Memory::ReadMem(ctx.esp + 0x8);

				if (s_instance_->m_currentCameraFOV == 1.6580627894f)
				{
					s_instance_->m_newCameraHFOV = fCurrentCameraHFOV * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newCameraHFOV = fCurrentCameraHFOV * s_instance_->m_aspectRatioScale;
				}

				FPU::FMUL(s_instance_->m_newCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2] + 10, 4); // NOP out the original instruction

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScansResult[FOV2] + 10, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = Memory::ReadMem(ctx.esp + 0x8);

				if (s_instance_->m_currentCameraFOV == 1.6580627894f)
				{
					s_instance_->m_newCameraVFOV = fCurrentCameraVFOV * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newCameraVFOV = fCurrentCameraVFOV * s_instance_->m_aspectRatioScale;
				}

				FPU::FMUL(s_instance_->m_newCameraVFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	HMODULE m_chromeEngineDllModule = nullptr;
	std::string m_chromeEngineDllModuleName = "";

	float m_currentCameraFOV = 0.0f;
	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	inline static PetRacerFix* s_instance_ = nullptr;
};

static std::unique_ptr<PetRacerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<PetRacerFix>(hModule);
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