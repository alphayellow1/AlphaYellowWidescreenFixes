#include "..\..\common\FixBase.hpp"

class HummerFix final : public FixBase
{
public:
	explicit HummerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~HummerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "4x4HummerFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "4x4 Hummer";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Hummer.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		gameDll = Memory::GetHandle("ChromeEngine2.dll");
		gameDllName = Memory::GetModuleName(gameDll);

		auto resolutionScanResult = Memory::PatternScan(gameDll, "8B 74 24 ?? 57 8B 7C 24 ?? 56 57 68");
		if (resolutionScanResult)
		{
			spdlog::info("Resolution Scan: Address is {:s}+{:x}", gameDllName.c_str(), resolutionScanResult - (std::uint8_t*)gameDll);

			m_resolutionWidthHook = safetyhook::create_mid(resolutionScanResult + 5, [](SafetyHookContext& ctx)
			{
				s_instance_->iCurrentWidth = Memory::ReadMem(ctx.esp + 0xC);

				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->iCurrentWidth) / static_cast<float>(s_instance_->iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});

			m_resolutionHeightHook = safetyhook::create_mid(resolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->iCurrentHeight = Memory::ReadMem(ctx.esp + 0xC);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution scan memory address.");
			return;
		}

		auto InARaceTriggerInstructionScanResult = Memory::PatternScan(gameDll, "39 1D ?? ?? ?? ?? 75 66 8B 0D ?? ?? ?? ?? 53");
		if (InARaceTriggerInstructionScanResult)
		{
			spdlog::info("In A Race Trigger Instruction: Address is {:s}+{:x}", gameDllName.c_str(), InARaceTriggerInstructionScanResult - (std::uint8_t*)gameDll);

			TriggerValueAddress = Memory::GetPointerFromAddress(InARaceTriggerInstructionScanResult + 2, Memory::PointerMode::Absolute);

			pInRaceFlag = reinterpret_cast<uint32_t*>(TriggerValueAddress);
		}
		else
		{
			spdlog::error("Failed to locate in a race trigger instruction memory address.");
			return;
		}

		auto CameraFOVInstructionsScanResult = Memory::PatternScan(gameDll, "D9 44 24 ?? D8 C9 D9 5C 24 ?? D8 C9 D8 4C 24");
		if (CameraFOVInstructionsScanResult)
		{
			spdlog::info("Camera FOV Instructions Scan: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVInstructionsScanResult - (std::uint8_t*)gameDll);

			Memory::WriteNOPs(CameraFOVInstructionsScanResult, 4); // NOP out the original instruction

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraHFOV = Memory::ReadMem(ctx.esp + 0xC);

				s_instance_->bInRace = (*s_instance_->pInRaceFlag == 1);

				if (s_instance_->bInRace == 1)
				{
					s_instance_->m_newCameraHFOV = fCurrentCameraHFOV * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newCameraHFOV = fCurrentCameraHFOV * s_instance_->m_aspectRatioScale;
				}

				FPU::FLD(s_instance_->m_newCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult + 12, 4); // NOP out the original instruction

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVInstructionsScanResult + 12, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = Memory::ReadMem(ctx.esp + 0xC);

				s_instance_->bInRace = (*s_instance_->pInRaceFlag == 1);

				if (s_instance_->bInRace == 1)
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
		else
		{
			spdlog::error("Failed to locate camera FOV instructions scan memory address.");
			return;
		}
	}

private:
	HMODULE gameDll = nullptr;
	std::string gameDllName;

	uintptr_t TriggerValueAddress;
	uint32_t* pInRaceFlag;
	bool bInRace;

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionWidthHook;
	SafetyHookMid m_resolutionHeightHook;
	SafetyHookMid m_cameraHFOVHook;
	SafetyHookMid m_cameraVFOVHook;

	int iCurrentWidth = 0;
	int iCurrentHeight = 0;

	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	inline static HummerFix* s_instance_ = nullptr;
};

static std::unique_ptr<HummerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<HummerFix>(hModule);

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