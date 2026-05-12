#include "..\..\common\FixBase.hpp"

class NicktoonsFix final : public FixBase
{
public:
	explicit NicktoonsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~NicktoonsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "NicktoonsRacingWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Nicktoons Racing";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "launch.exe") ||
			Util::stringcmp_caseless(exeName, "toonsDX7.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "launch.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "83 7B ?? ?? 0F 85 ?? ?? ?? ?? 8B B5");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 317);
				Memory::PatchBytes(ResolutionListUnlockScanResult, "\x8B\x0B\x8B\x43\x04");
			}
			else
			{
				spdlog::info("Failed to locate the resolution list unlock scan memory address.");
				return;
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "toonsDX7.exe"))
		{
			auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 04 95 ?? ?? ?? ?? D8 04 85");
			if (CameraFOVScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

				m_cameraFOVOffset = Memory::GetPointerFromAddress(CameraFOVScanResult + 3, Memory::PointerMode::Absolute);

				Memory::WriteNOPs(CameraFOVScanResult, 7);

				m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVInstructionsMidHook(ctx.edx * 0x4 + s_instance_->m_cameraFOVOffset);
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;

	SafetyHookMid m_cameraFOVHook{};

	uintptr_t m_cameraFOVOffset = 0;

	void CameraFOVInstructionsMidHook(uintptr_t SourceAddress)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(SourceAddress);
		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;
		FPU::FLD(m_newCameraFOV);
	}

	inline static NicktoonsFix* s_instance_ = nullptr;
};

static std::unique_ptr<NicktoonsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<NicktoonsFix>(hModule);
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