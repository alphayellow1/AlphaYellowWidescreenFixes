#include "..\..\common\FixBase.hpp"

class MonetFix final : public FixBase
{
public:
	explicit MonetFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MonetFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MonetTheMysteryOfTheOrangeryFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Monet: The Mystery of the Orangery";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "MissionMonet.exe");
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
		m_x3dDllModule = Memory::GetHandle("x3d.dll");
		m_x3dDllModuleName = Memory::GetModuleName(m_x3dDllModule);

		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto AspectRatioScansResult = Memory::PatternScan(m_x3dDllModule, "D8 48 ?? D9 40 ?? D8 49 ?? 8D 4C 24", "D8 48 ?? D9 40 ?? D8 49 ?? C7 44 24",
		"D8 48 ?? D9 40 ?? D8 4E ?? 8D 44 24", "D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8B 46",
		"D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8D 44 24");

		auto CameraFOVScansResult = Memory::PatternScan(m_x3dDllModule, "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 49",
		"DC 3D ?? ?? ?? ?? D9 C0 D8 48", "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? 8D 44 24",
		"DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8B 46",
		"DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8D 44 24");

		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instructions 1 Scan: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Aspect Ratio Instructions 2 Scan: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Aspect Ratio Instructions 3 Scan: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Aspect Ratio Instructions 4 Scan: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Aspect Ratio Instructions 5 Scan: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), AspectRatioScansResult[AR5] - (std::uint8_t*)m_x3dDllModule);

			m_newResX2 = (float)m_newResX;
			m_newResY2 = (float)m_newResY;

			Memory::WriteNOPs(AspectRatioScansResult, AR1, AR5, 0, 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], AspectRatioMidHook);
			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], AspectRatioMidHook);
			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], AspectRatioMidHook);
			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], AspectRatioMidHook);
			m_aspectRatio5Hook = safetyhook::create_mid(AspectRatioScansResult[AR5], AspectRatioMidHook);
		}

		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)m_x3dDllModule);
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)m_x3dDllModule);

			m_newCameraFOV = (1.0 / (double)m_aspectRatioScale) / m_fovFactor;

			Memory::Write(CameraFOVScansResult, FOV1, FOV5, 2, &m_newCameraFOV);
		}
	}

private:
	HMODULE m_x3dDllModule = nullptr;
	std::string m_x3dDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_aspectRatio5Hook{};

	static void AspectRatioMidHook(SafetyHookContext& ctx)
	{
		FPU::FMUL(s_instance_->m_newResX2);
		FPU::FLD(s_instance_->m_newResY2);
	}

	static void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		s_instance_->m_newCameraFOV = (1.0 / (double)s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor;

		FPU::FDIVR(s_instance_->m_newCameraFOV);
	}

	float m_newResX2 = 0.0f;
	float m_newResY2 = 0.0f;
	double m_newCameraFOV = 0.0;
	double m_fovFactor = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
	};

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2,
		AR3,
		AR4,
		AR5
	};

	enum CameraHFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7
	};

	inline static MonetFix* s_instance_ = nullptr;
};

static std::unique_ptr<MonetFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<MonetFix>(hModule);
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