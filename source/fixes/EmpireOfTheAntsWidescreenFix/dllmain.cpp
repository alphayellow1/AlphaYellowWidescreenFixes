#include "..\..\common\FixBase.hpp"

class EmpireOfTheAntsFix final : public FixBase
{
public:
	explicit EmpireOfTheAntsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~EmpireOfTheAntsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EmpireOfTheAntsWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Empire of the Ants";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Game.exe") || 
		Util::stringcmp_caseless(exeName, "InitVid.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "InitVid.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "0F 8C ?? ?? ?? ?? DB 44 24");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 6);
				Memory::WriteNOPs(ResolutionListUnlockScanResult + 38, 2);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Game.exe"))
		{
			auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "7C ?? DB 44 24 ?? DC 0D", "8B 4C 24 ?? 8B 44 24 ?? 89 0D");
			if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionScansResult[ListUnlock], 2);
				Memory::WriteNOPs(ResolutionScansResult[ListUnlock] + 26, 2);

				m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
				{
					uint32_t& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);
					uint32_t& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->m_resolutionHook.disable();
				});
			}

			m_x3dDllModule = Memory::GetHandle("x3d.dll");
			m_x3dDllModuleName = Memory::GetModuleName(m_x3dDllModule);

			auto CameraFOVScansResult = Memory::PatternScan(m_x3dDllModule, "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 49",
			"DC 3D ?? ?? ?? ?? D9 C0 D8 48", "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? 8D 44 24",
			"DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8B 46",
			"DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8D 44 24",
			"D9 46 ?? D8 0D ?? ?? ?? ?? D8 0D", "D9 44 24 ?? D9 F2");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), CameraFOVScansResult[FOV7] - (std::uint8_t*)m_x3dDllModule);

				Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

				m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], CameraFOVMidHook);

				Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

				m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], CameraFOVMidHook);

				Memory::WriteNOPs(CameraFOVScansResult[FOV3], 6);

				m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], CameraFOVMidHook);

				Memory::WriteNOPs(CameraFOVScansResult[FOV4], 6);

				m_cameraFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[FOV4], CameraFOVMidHook);

				Memory::WriteNOPs(CameraFOVScansResult[FOV5], 6);

				m_cameraFOV5Hook = safetyhook::create_mid(CameraFOVScansResult[FOV5], CameraFOVMidHook);

				Memory::WriteNOPs(CameraFOVScansResult[FOV6], 3);

				m_cameraFOV6Hook = safetyhook::create_mid(CameraFOVScansResult[FOV6], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV6 = Memory::ReadMem(ctx.esi + 0x50);
					s_instance_->m_newCameraFOV6 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV6, 1.0f / s_instance_->m_aspectRatioScale) / (float)s_instance_->m_fovFactor;
					FPU::FLD(s_instance_->m_newCameraFOV6);
				});

				Memory::WriteNOPs(CameraFOVScansResult[FOV7], 4);

				m_newCameraFOV7 = 5.0f;

				m_cameraFOV7Hook = safetyhook::create_mid(CameraFOVScansResult[FOV7], [](SafetyHookContext& ctx)
				{
					FPU::FLD(s_instance_->m_newCameraFOV7);
				});
			}
		}
	}

private:
	HMODULE m_x3dDllModule = nullptr;
	std::string m_x3dDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};
	SafetyHookMid m_cameraFOV5Hook{};
	SafetyHookMid m_cameraFOV6Hook{};
	SafetyHookMid m_cameraFOV7Hook{};

	static void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		s_instance_->m_newCameraFOV = (1.0 / (double)s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor;

		FPU::FDIVR(s_instance_->m_newCameraFOV);
	}

	double m_newCameraFOV = 0.0;
	float m_newCameraFOV6 = 0.0f;
	float m_newCameraFOV7 = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
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

	inline static EmpireOfTheAntsFix* s_instance_ = nullptr;
};

static std::unique_ptr<EmpireOfTheAntsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<EmpireOfTheAntsFix>(hModule);
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