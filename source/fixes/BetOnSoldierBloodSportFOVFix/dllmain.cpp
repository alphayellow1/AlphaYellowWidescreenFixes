#include "..\..\common\FixBase.hpp"

class BetOnSoldierBloodSportFix final : public FixBase
{
public:
	explicit BetOnSoldierBloodSportFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BetOnSoldierBloodSportFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BetOnSoldierBloodSportFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Bet on Soldier: Blood Sport";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "BoS.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "StandardCameraFOVFactor", m_standardCameraFOVFactor);
		inipp::get_value(ini.sections["Settings"], "SuitCameraFOVFactor", m_suitCameraFOVFactor);
		inipp::get_value(ini.sections["Settings"], "StandardZoomFOVFactor", m_standardZoomFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ShieldZoomFOVFactor", m_shieldZoomFOVFactor);
		inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", m_weaponFOVFactor);
		spdlog_confparse(m_standardCameraFOVFactor);
		spdlog_confparse(m_suitCameraFOVFactor);
		spdlog_confparse(m_standardZoomFOVFactor);
		spdlog_confparse(m_shieldZoomFOVFactor);
		spdlog_confparse(m_weaponFOVFactor);
	}

	void ApplyFix() override
	{
		m_kteCoreDllModule = Memory::GetHandle("kte_core.dll");
		m_bosDllModule = Memory::GetHandle("Bos.dll");
		m_kteDX9DllModule = Memory::GetHandle("kte_dx9.dll");
		m_nxFluidRenderDllModule = Memory::GetHandle("NxFluidRender.DLL");
		m_kteCoreDllModuleName = Memory::GetModuleName(m_kteCoreDllModule);
		m_bosDllModuleName = Memory::GetModuleName(m_bosDllModule);
		m_kteDX9DllModuleName = Memory::GetModuleName(m_kteDX9DllModule);
		m_nxFluidRenderDllModuleName = Memory::GetModuleName(m_nxFluidRenderDllModule);

		auto ResolutionScanResult = Memory::PatternScan(m_nxFluidRenderDllModule, "8B 44 24 ?? 8B 4C 24 ?? 89 86");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_nxFluidRenderDllModuleName.c_str(), ResolutionScanResult - (std::uint8_t*)m_nxFluidRenderDllModule);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x2C);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x30);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScansResult = Memory::PatternScan(m_kteDX9DllModule, "8B 8D ?? ?? ?? ?? 8B 95 ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? 51 8B 8D ?? ?? ?? ?? 52 50 51 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 13 8D 44 24 ?? 50 8B CB FF 92 ?? ?? ?? ?? 83 BD ?? ?? ?? ?? ?? 75 ?? 8B 13 68 ?? ?? ?? ?? 8B CB FF 92 ?? ?? ?? ?? 8B F8 85 FF 74 ?? 8B 0D ?? ?? ?? ?? 8B 01 68 ?? ?? ?? ?? FF 50 ?? 8B F0 85 F6 74 ?? 8B 16 8B CE FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? 8B CE 89 B5 ?? ?? ?? ?? 8B 11 57 FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 0F 84 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B F8 8D 83 ?? ?? ?? ?? 50 8D 8B ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? 51 8B CE 89 7C 24 ?? FF 15 ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D 54 24 ?? F3 ?? 8B 8B ?? ?? ?? ?? 52 FF 15 ?? ?? ?? ?? 8B 54 24 ?? 8B F0 8D 7A ?? B9 ?? ?? ?? ?? F3 ?? 8D BA ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? F3 ?? 8B 8D ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 48 ?? 85 C9 DB 40 ?? 7D ?? D8 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? D9 C0 D9 98 ?? ?? ?? ?? D9 85 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D8 F9 D9 98 ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? 8B 11 DD D8 FF 92 ?? ?? ?? ?? 8B 03 8B CB FF 90 ?? ?? ?? ?? 8B 08 8D 94 24 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 45 ?? 8D B5 ?? ?? ?? ?? 56 8B CD FF 90 ?? ?? ?? ?? 8B 13 56 8B CB FF 92 ?? ?? ?? ?? 80 7C 24 ?? ?? 74 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 81 EC",
		m_kteCoreDllModule, "D8 B6 ?? ?? ?? ?? D9 E8");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", m_kteDX9DllModuleName.c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)m_kteDX9DllModule);
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", m_kteCoreDllModuleName.c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)m_kteCoreDllModule);

			Memory::WriteNOPs(AspectRatioScansResult[AR1], 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], [](SafetyHookContext& ctx)
			{
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR2], 6);

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], [](SafetyHookContext& ctx)
			{
				FPU::FDIV(s_instance_->m_newAspectRatio);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(
		m_kteDX9DllModule, "8B 8D ?? ?? ?? ?? 52 50 51 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 13 8D 44 24 ?? 50 8B CB FF 92 ?? ?? ?? ?? 83 BD ?? ?? ?? ?? ?? 75 ?? 8B 13 68 ?? ?? ?? ?? 8B CB FF 92 ?? ?? ?? ?? 8B F8 85 FF 74 ?? 8B 0D ?? ?? ?? ?? 8B 01 68 ?? ?? ?? ?? FF 50 ?? 8B F0 85 F6 74 ?? 8B 16 8B CE FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? 8B CE 89 B5 ?? ?? ?? ?? 8B 11 57 FF 52 ?? 8B 8D ?? ?? ?? ?? 85 C9 0F 84 ?? ?? ?? ?? 8B 01 FF 50 ?? 8B F8 8D 83 ?? ?? ?? ?? 50 8D 8B ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? 51 8B CE 89 7C 24 ?? FF 15 ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D 54 24 ?? F3 ?? 8B 8B ?? ?? ?? ?? 52 FF 15 ?? ?? ?? ?? 8B 54 24 ?? 8B F0 8D 7A ?? B9 ?? ?? ?? ?? F3 ?? 8D BA ?? ?? ?? ?? B9 ?? ?? ?? ?? 8D B3 ?? ?? ?? ?? F3 ?? 8B 8D ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 48 ?? 85 C9 DB 40 ?? 7D ?? D8 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? D9 C0 D9 98 ?? ?? ?? ?? D9 85 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D8 F9 D9 98 ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? 8B 11 DD D8 FF 92 ?? ?? ?? ?? 8B 03 8B CB FF 90 ?? ?? ?? ?? 8B 08 8D 94 24 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 45 ?? 8D B5 ?? ?? ?? ?? 56 8B CD FF 90 ?? ?? ?? ?? 8B 13 56 8B CB FF 92 ?? ?? ?? ?? 80 7C 24 ?? ?? 74 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 81 EC", "D9 85 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 F2 DD D8 D8 F9 D9 98 ?? ?? ?? ?? 8B 8D ?? ?? ?? ?? 8B 11 DD D8 FF 92 ?? ?? ?? ?? 8B 03 8B CB FF 90 ?? ?? ?? ?? 8B 08 8D 94 24 ?? ?? ?? ?? 52 50 FF 51 ?? 8B 45 ?? 8D B5 ?? ?? ?? ?? 56 8B CD FF 90 ?? ?? ?? ?? 8B 13 56 8B CB FF 92 ?? ?? ?? ?? 80 7C 24 ?? ?? 74 ?? 8B 8D ?? ?? ?? ?? 85 C9 74 ?? 8B 01 FF 50 ?? C7 85 ?? ?? ?? ?? ?? ?? ?? ?? 5F 5E 5D 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 81 EC",
		m_kteCoreDllModule, "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8D 47",
		m_bosDllModule, "A1 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 53 56 8B F1 8B 0D ?? ?? ?? ??", "D9 41 40 C2 04 00 D9 41 3C");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General FOV Instruction 1: Address is {:s}+{:x}", m_kteDX9DllModuleName.c_str(), CameraFOVScansResult[General1] - (std::uint8_t*)m_kteDX9DllModule);
			spdlog::info("General FOV Instruction 2: Address is {:s}+{:x}", m_kteDX9DllModuleName.c_str(), CameraFOVScansResult[General2] - (std::uint8_t*)m_kteDX9DllModule);
			spdlog::info("General FOV Instruction 3: Address is {:s}+{:x}", m_kteCoreDllModuleName.c_str(), CameraFOVScansResult[General3] - (std::uint8_t*)m_kteCoreDllModule);
			spdlog::info("Hipfire Camera/Shield Zoom/Standard Zoom FOV Instructions Scan: Address is {:s}+{:x}", m_bosDllModuleName.c_str(), CameraFOVScansResult[HipfireAndZoom] - (std::uint8_t*)m_bosDllModule);
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", m_bosDllModuleName.c_str(), CameraFOVScansResult[Weapon] - (std::uint8_t*)m_bosDllModule);

			// Camera FOV Instruction 1 Hook
			Memory::WriteNOPs(CameraFOVScansResult[General1], 6);

			m_generalFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[General1], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGeneralFOV1 = Memory::ReadMem(ctx.ebp + 0xD4);
				s_instance_->m_newGeneralFOV1 = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentGeneralFOV1, s_instance_->m_aspectRatioScale);
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newGeneralFOV1);
			});

			// Camera FOV Instruction 2 Hook
			Memory::WriteNOPs(CameraFOVScansResult[General2], 6);

			m_generalFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[General2], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGeneralFOV2 = Memory::ReadMem(ctx.ebp + 0xD4);
				s_instance_->m_newGeneralFOV2 = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentGeneralFOV2, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newGeneralFOV2);
			});

			// Camera FOV Instruction 3 Hook
			Memory::WriteNOPs(CameraFOVScansResult[General3], 6);

			m_generalFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[General3], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGeneralFOV3 = Memory::ReadMem(ctx.esi + 0xD4);
				s_instance_->m_newGeneralFOV3 = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentGeneralFOV3, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newGeneralFOV3);
			});

			// Hipfire Camera/Shield Zoom/Standard Zoom FOV Instructions Hook
			m_hipfireCameraFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[HipfireAndZoom] + 1, Memory::PointerMode::Absolute);
			m_shieldZoomFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[HipfireAndZoom] + 7, Memory::PointerMode::Absolute);
			m_standardZoomFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[HipfireAndZoom] + 17, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[HipfireAndZoom], 11);

			m_hipfireCameraAndShieldZoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[HipfireAndZoom], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHipfireFOV = Memory::ReadMem(s_instance_->m_hipfireCameraFOVAddress);
				s_instance_->m_newHipfireCameraFOV = s_instance_->m_currentHipfireFOV * s_instance_->m_standardCameraFOVFactor;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newHipfireCameraFOV);

				s_instance_->m_currentShieldZoomFOV = Memory::ReadMem(s_instance_->m_shieldZoomFOVAddress);
				s_instance_->m_newShieldZoomFOV = s_instance_->m_currentShieldZoomFOV / s_instance_->m_shieldZoomFOVFactor;
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newShieldZoomFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[HipfireAndZoom] + 15, 6);

			m_standardCameraZoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[HipfireAndZoom] + 15, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentStandardZoomFOV = Memory::ReadMem(s_instance_->m_standardZoomFOVAddress);
				s_instance_->m_newStandardZoomFOV = s_instance_->m_currentStandardZoomFOV / s_instance_->m_standardZoomFOVFactor;
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newStandardZoomFOV);
			});

			// Weapon FOV Instruction Hook
			Memory::WriteNOPs(CameraFOVScansResult[Weapon] + 6, 3);

			m_weaponHipfireFOVHook = safetyhook::create_mid(CameraFOVScansResult[Weapon] + 6, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWeaponHipfireFOV = Memory::ReadMem(ctx.ecx + 0x3C);
				s_instance_->m_newWeaponHipfireFOV = s_instance_->m_currentWeaponHipfireFOV * s_instance_->m_weaponFOVFactor;
				FPU::FLD(s_instance_->m_newWeaponHipfireFOV);
			});
		}
	}

private:
	HMODULE m_kteCoreDllModule = nullptr;
	HMODULE m_bosDllModule = nullptr;
	HMODULE m_kteDX9DllModule = nullptr;
	HMODULE m_nxFluidRenderDllModule = nullptr;
	std::string m_kteCoreDllModuleName = "";
	std::string m_bosDllModuleName = "";
	std::string m_kteDX9DllModuleName = "";
	std::string m_nxFluidRenderDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_generalFOV1Hook{};
	SafetyHookMid m_generalFOV2Hook{};
	SafetyHookMid m_generalFOV3Hook{};
	SafetyHookMid m_hipfireCameraAndShieldZoomFOVHook{};
	SafetyHookMid m_standardCameraZoomFOVHook{};
	SafetyHookMid m_weaponHipfireFOVHook{};

	float m_standardCameraFOVFactor = 0.0f;
	float m_suitCameraFOVFactor = 0.0f;
	float m_standardZoomFOVFactor = 0.0f;
	float m_shieldZoomFOVFactor = 0.0f;
	float m_weaponFOVFactor = 0.0f;

	float m_currentGeneralFOV1 = 0.0f;
	float m_currentGeneralFOV2 = 0.0f;
	float m_currentGeneralFOV3 = 0.0f;
	float m_newGeneralFOV1 = 0.0f;
	float m_newGeneralFOV2 = 0.0f;
	float m_newGeneralFOV3 = 0.0f;

	float m_currentHipfireFOV = 0.0f;
	float m_currentShieldZoomFOV = 0.0f;
	float m_currentStandardZoomFOV = 0.0f;
	float m_currentWeaponHipfireFOV = 0.0f;
	float m_newHipfireCameraFOV = 0.0f;
	float m_newWeaponHipfireFOV = 0.0f;
	float m_newShieldZoomFOV = 0.0f;
	float m_newStandardZoomFOV = 0.0f;
	uintptr_t m_hipfireCameraFOVAddress = 0;
	uintptr_t m_shieldZoomFOVAddress = 0;
	uintptr_t m_standardZoomFOVAddress = 0;

	enum AspectRatioInstructionsScansIndices
	{
		AR1,
		AR2
	};

	enum CameraFOVInstructionsScansIndices
	{
		General1,
		General2,
		General3,
		HipfireAndZoom,
		Weapon
	};

	inline static BetOnSoldierBloodSportFix* s_instance_ = nullptr;
};

static std::unique_ptr<BetOnSoldierBloodSportFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BetOnSoldierBloodSportFix>(hModule);
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