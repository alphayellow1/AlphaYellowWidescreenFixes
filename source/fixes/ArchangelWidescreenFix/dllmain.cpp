#include "..\..\common\FixBase.hpp"

class ArchangelFix final : public FixBase
{
public:
	explicit ArchangelFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ArchangelFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ArchangelWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Archangel";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "game.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 8B BC 24 ?? ?? ?? ?? 8B 8C");
		if (ResolutionListUnlockScanResult)
		{
			Memory::WriteNOPs(ResolutionListUnlockScanResult, 6);
			Memory::WriteNOPs(ResolutionListUnlockScanResult + 40, 6);
			Memory::PatchBytes(ResolutionListUnlockScanResult + 48, "\xE9\x67\x00\x00\x00\x90");
		}
		else
		{
			spdlog::error("Failed to locate resolution list unlock scan memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 81 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8A");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.ecx + 0xD4);
				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
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
	SafetyHookMid m_cameraFOVHook{};

	inline static ArchangelFix* s_instance_ = nullptr;
};

static std::unique_ptr<ArchangelFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ArchangelFix>(hModule);
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