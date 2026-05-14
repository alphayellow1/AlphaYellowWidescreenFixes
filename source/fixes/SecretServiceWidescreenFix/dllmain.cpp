#include "..\..\common\FixBase.hpp"

class SecretServiceFix final : public FixBase
{
public:
	explicit SecretServiceFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SecretServiceFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SecretServiceWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Secret Service";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ss.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "HipfireFOVFactor", m_hipfireFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		spdlog_confparse(m_hipfireFOVFactor);
		spdlog_confparse(m_zoomFactor);
	}

	void ApplyFix() override
	{
		HMODULE CloakNTEngineDllModule = Memory::GetHandle("CloakNTEngine.dll");
		std::string CloakNTEngineDllName = Memory::GetModuleName(CloakNTEngineDllModule);

		auto ResolutionListUnlockScanResult = Memory::PatternScan(CloakNTEngineDllModule, "83 3C BD ?? ?? ?? ?? ?? 74");
		if (ResolutionListUnlockScanResult)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)CloakNTEngineDllModule);

			Memory::WriteNOPs(ResolutionListUnlockScanResult, 30);
		}
		else
		{
			spdlog::error("Failed to locate resolution list unlock scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(CloakNTEngineDllModule, "D9 05 ?? ?? ?? ?? 89 85 ?? ?? ?? ?? 8D 4D", "D9 05 ?? ?? ?? ?? D9 1C 24 D9 04 24 59 C3 CC",
		"D9 05 ?? ?? ?? ?? DC 05 ?? ?? ?? ?? C7 06 ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? D9 5E ?? C6 46 ?? ?? 8B C6",
		"D9 05 ?? ?? ?? ?? 8B 54 24 ?? D9 96", "D9 05 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? EB ?? 8B 46",
		"DB 83 F9 ?? 0F 8C ?? ?? ?? ?? 83 F9 ?? 0F 8F ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? D9 05 ?? ?? ?? ??",
		"D9 05 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 8B 4C 24 ?? 89 96 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? 8B 54 24 ?? 89 86",
		"D9 05 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? 8B 8E", "88 ?? ?? ?? ?? 83 F9 ?? 0F 8C ?? ?? ?? ?? 83 F9 ?? 0F 8F ?? ?? ?? ?? 8B 80 ?? ?? ?? ?? D9 05 ?? ?? ?? ??",
		"D9 05 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 89 8E ?? ?? ?? ?? 8B 4C 24 ?? 89 96 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? 8B 54 24 ?? 33 DB",
		"D9 46 ?? D9 1C 24 E8 ?? ?? ?? ?? D9 EE D8 5C 24");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Actor Initialize FOV Instruction: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[ActorInitialize] - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Hipfire FOV Instruction: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[Hipfire] - (std::uint8_t*)CloakNTEngineDllModule);			
			spdlog::info("Sprint FOV Instruction 1: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[Sprint1] - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Sprint FOV Instruction 2: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[Sprint2] - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Change Weapon FOV Instruction 1: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[ChangeWeapon1] - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Change Weapon FOV Instruction 2: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[ChangeWeapon2] + 25 - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Change Weapon FOV Instruction 3: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[ChangeWeapon3] - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Weapon Show FOV Instruction: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[WeaponShow] - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Reset Weapon FOV Instruction 1: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[ResetWeapon1] + 29 - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Reset Weapon FOV Instruction 2: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[ResetWeapon2] - (std::uint8_t*)CloakNTEngineDllModule);
			spdlog::info("Zoom FOV Instruction: Address is {:s}+{:x}", CloakNTEngineDllName.c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)CloakNTEngineDllModule);

			m_newHipfireFOV = m_originalHipfireFOV * m_hipfireFOVFactor;

			Memory::Write(CameraFOVScansResult[ActorInitialize] + 2, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[Hipfire] + 2, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[Sprint1] + 2, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[Sprint2] + 2, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[ChangeWeapon1] + 2, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[ChangeWeapon2] + 27, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[ChangeWeapon3] + 2, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[WeaponShow] + 2, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[ResetWeapon1] + 31, &m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[ResetWeapon2] + 2, &m_newHipfireFOV);

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 3);
			m_zoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = Memory::ReadMem(ctx.esi + 0x34);
				s_instance_->m_newZoomFOV = fCurrentZoomFOV / s_instance_->m_zoomFactor;
				FPU::FLD(s_instance_->m_newZoomFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	static constexpr float m_originalHipfireFOV = 1.22173059f;

	float m_hipfireFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;

	float m_newHipfireFOV = 0.0f;
	float m_newZoomFOV = 0.0f;

	SafetyHookMid m_zoomFOVHook{};

	enum CameraFOVInstructionsIndices
	{
		ActorInitialize,
		Hipfire,		
		Sprint1,
		Sprint2,
		ChangeWeapon1,
		ChangeWeapon2,
		ChangeWeapon3,
		WeaponShow,
		ResetWeapon1,
		ResetWeapon2,
		Zoom
	};

	inline static SecretServiceFix* s_instance_ = nullptr;
};

static std::unique_ptr<SecretServiceFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<SecretServiceFix>(hModule);
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