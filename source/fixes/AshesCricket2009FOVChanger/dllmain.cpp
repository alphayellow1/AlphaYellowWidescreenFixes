#include "..\..\common\FixBase.hpp"

class AshesCricket2009Fix final : public FixBase
{
public:
	explicit AshesCricket2009Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AshesCricket2009Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AshesCricket2009FOVChanger";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Ashes Cricket 2009";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Cricket2009.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "F3 0F 11 41 50 59 C2 04 00 CC 55 8B EC 83 E4 F0 83 EC 54", "D9 06 DC 0D ?? ?? ?? ?? E8");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 5);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = ctx.xmm0.f32[0];
				s_instance_->m_newCameraFOV1 = fCurrentCameraFOV1 * s_instance_->m_fovFactor;
				*reinterpret_cast<float*>(ctx.ecx + 0x50) = s_instance_->m_newCameraFOV1;
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 2);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.esi);
				s_instance_->m_newCameraFOV2 = fCurrentCameraFOV2 * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV2);
			});
		}
	}

private:
	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	inline static AshesCricket2009Fix* s_instance_ = nullptr;
};

static std::unique_ptr<AshesCricket2009Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AshesCricket2009Fix>(hModule);
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