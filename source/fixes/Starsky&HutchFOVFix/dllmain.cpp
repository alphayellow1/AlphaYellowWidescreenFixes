#include "..\..\common\FixBase.hpp"

class StarskyFix final : public FixBase
{
public:
	explicit StarskyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~StarskyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Starsky&HutchFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Starsky & Hutch";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "StarskyPC.exe");
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
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}

		auto CameraFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D9 55 ?? D8 35 ?? ?? ?? ?? 51",
		"C7 81 ?? ?? ?? ?? ?? ?? ?? ?? E9 ?? ?? ?? ?? 8B 55 ?? C7 42 ?? ?? ?? ?? ?? 8B 45 ?? C7 40 ?? ?? ?? ?? ?? 8B 4D ?? C7 41 ?? ?? ?? ?? ?? 8B 55");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)ExeModule());

			m_cameraFOV1Address = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[FOV1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOV1MidHook(ctx);
			});

			m_newCameraFOV2 = m_originalCameraFOV2 * m_fovFactor;

			Memory::Write(CameraFOVInstructionsScansResult[FOV2] + 6, m_newCameraFOV2);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOV1Hook{};

	uintptr_t m_cameraFOV1Address = 0;
	float m_newCameraFOV1;
	static constexpr float m_originalCameraFOV2 = 85.0f;
	float m_newCameraFOV2;

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	void CameraFOV1MidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV1 = Memory::ReadMem(m_cameraFOV1Address);

		m_newCameraFOV1 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV1, m_aspectRatioScale);

		FPU::FMUL(m_newCameraFOV1);
	}

	inline static StarskyFix* s_instance_ = nullptr;
};

static std::unique_ptr<StarskyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<StarskyFix>(hModule);
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