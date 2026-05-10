#include "..\..\common\FixBase.hpp"

class AliasUndergroundFix final : public FixBase
{
public:
	explicit AliasUndergroundFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AliasUndergroundFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AliasUndergroundFOVChanger";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Alias: Underground";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "javaw.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_dllModule = Memory::GetHandle("dmdx7.dll");

		auto CameraFOVInstructionScanResult = Memory::PatternScan(m_dllModule, "DD 84 24 ?? ?? ?? ?? DC 0D");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is dmdx7.dll+{:x}", CameraFOVInstructionScanResult - (std::uint8_t*)m_dllModule);

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 7);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				double& dCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0xD4);

				s_instance_->m_newCameraFOV = dCurrentCameraFOV * s_instance_->m_fovFactor;

				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	HMODULE m_dllModule = nullptr;

	SafetyHookMid m_cameraFOVHook{};

	double m_newCameraFOV = 0.0;
	double m_fovFactor = 0.0;

	inline static AliasUndergroundFix* s_instance_ = nullptr;
};

static std::unique_ptr<AliasUndergroundFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AliasUndergroundFix>(hModule);
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