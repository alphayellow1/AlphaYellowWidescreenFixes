#include "..\..\common\FixBase.hpp"

class StateOfEmergencyFix final : public FixBase
{
public:
	explicit StateOfEmergencyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~StateOfEmergencyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "StateOfEmergencyWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.8";
	}

	const char* TargetName() const override
	{
		return "State of Emergency";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "KaosPC.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "F6 C4 ?? 7A ?? 8B 44 24 ?? 45", "F6 C4 ?? 7A ?? 47", "8B 04 10 57 A3");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1], 5);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock2], 5);

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadMem(ctx.eax + ctx.edx);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight] + 19, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHeight = Memory::ReadMem(ctx.eax + ctx.ecx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? 75 ?? C7 44 24 ?? ?? ?? ?? ?? D9 44 24 ?? 8B 44 24",
		"D9 05 ?? ?? ?? ?? D8 4C 24 ?? C7 05");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR] + 10 - (std::uint8_t*)ExeModule());
			spdlog::info("HFOV Culling Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HFOVCulling] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[AR], 8);
			Memory::WriteNOPs(AspectRatioScansResult[AR] + 10, 8);
			Memory::WriteNOPs(AspectRatioScansResult[HFOVCulling], 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esp + 0x48) = s_instance_->m_newAspectRatio;
			});

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR] + 10, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esp + 0x48) = s_instance_->m_newAspectRatio;
			});

			m_hfovCullingHook = safetyhook::create_mid(AspectRatioScansResult[HFOVCulling] + 10, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newHFOVCulling = 1.0f / s_instance_->m_aspectRatioScale;

				FPU::FLD(s_instance_->m_newHFOVCulling);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53 C7 05");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult))
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			m_newCameraFOV1 = 810.0f / m_fovFactor;

			m_newCameraFOV2 = 900.0f / m_fovFactor;

			Memory::Write(CameraFOVScansResult[FOV1] + 1, m_newCameraFOV1);

			Memory::Write(CameraFOVScansResult[FOV2] + 1, m_newCameraFOV1);

			Memory::Write(CameraFOVScansResult[FOV3] + 1, m_newCameraFOV2);			
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_hfovCullingHook{};

	uint32_t m_currentWidth = 0;
	uint32_t m_currentHeight = 0;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;
	float m_newHFOVCulling = 0.0f;

	enum ResolutionListsIndex
	{
		ResListUnlock1,
		ResListUnlock2,
		ResWidthHeight
	};

	enum AspectRatioInstructionsIndex
	{
		AR,
		HFOVCulling
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2,
		FOV3
	};

	inline static StateOfEmergencyFix* s_instance_ = nullptr;
};

static std::unique_ptr<StateOfEmergencyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<StateOfEmergencyFix>(hModule);
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