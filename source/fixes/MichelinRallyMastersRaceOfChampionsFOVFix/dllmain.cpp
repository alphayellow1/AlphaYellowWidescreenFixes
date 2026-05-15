#include "..\..\common\FixBase.hpp"

class RallyMastersFix final : public FixBase
{
public:
	explicit RallyMastersFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RallyMastersFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MichelinRallyMastersRaceOfChampionsFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Michelin Rally Masters: Race of Champions";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "RallyMasters.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 8B 4C 24 ?? 89 44 24 ?? 89 74 24");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x10);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x14);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}		

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "DC 3D ?? ?? ?? ?? D9 51", "A1 ?? ?? ?? ?? 56 57 8B F9", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B CE 68");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Outside Views Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[OutsideViews] - (std::uint8_t*)ExeModule());
			spdlog::info("Cockpit Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cockpit] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 6);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newOverallFOV = 1.0 / (double)s_instance_->m_aspectRatioScale;

				FPU::FDIVR(s_instance_->m_newOverallFOV);
			});

			OutsideViewsFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[OutsideViews] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[OutsideViews], 5);

			m_outsideViewsFOVHook = safetyhook::create_mid(CameraFOVScansResult[OutsideViews], [](SafetyHookContext& ctx)
			{
				float& fCurrentOutsideViewsFOV = Memory::ReadMem(s_instance_->OutsideViewsFOVAddress);
				s_instance_->m_newOutsideViewsFOV = fCurrentOutsideViewsFOV * s_instance_->m_fovFactor;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newOutsideViewsFOV);
			});

			m_newCockpitFOV = m_originalCockpitFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult[Cockpit] + 1, m_newCockpitFOV);
		}

		auto HUDAspectRatioScansResult = Memory::PatternScan(ExeModule(), "BA ?? ?? ?? ?? 2B D6 2B D7", "D8 0D ?? ?? ?? ?? 89 16");
		if (Memory::AreAllSignaturesValid(HUDAspectRatioScansResult) == true)
		{
			spdlog::info("HUD Width Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDAspectRatioScansResult[HUDWidth] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Vertical Position Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDAspectRatioScansResult[HUDVertical] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(HUDAspectRatioScansResult[HUDWidth], 5);

			m_hudWidthHook = safetyhook::create_mid(HUDAspectRatioScansResult[HUDWidth], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newHUDWidth = (uint32_t)(s_instance_->m_originalHUDWidth * s_instance_->m_aspectRatioScale);

				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newHUDWidth);
			});

			m_hudVerticalPositionAddress = Memory::GetPointerFromAddress(HUDAspectRatioScansResult[HUDVertical] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(HUDAspectRatioScansResult[HUDVertical], 6);

			m_hudVerticalPositionHook = safetyhook::create_mid(HUDAspectRatioScansResult[HUDVertical], [](SafetyHookContext& ctx)
			{
				float& fCurrentHUDVerticalPosition = Memory::ReadMem(s_instance_->m_hudVerticalPositionAddress);

				s_instance_->m_newHUDVerticalPosition = fCurrentHUDVerticalPosition / s_instance_->m_aspectRatioScale;

				FPU::FMUL(s_instance_->m_newHUDVerticalPosition);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCockpitFOV = 80.0f;
	static constexpr float m_originalHUDWidth = 320;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_outsideViewsFOVHook{};
	SafetyHookMid m_hudWidthHook{};
	SafetyHookMid m_hudVerticalPositionHook{};

	uintptr_t OutsideViewsFOVAddress = 0;
	uintptr_t m_hudVerticalPositionAddress = 0;

	double m_newOverallFOV = 0.0;
	float m_newOutsideViewsFOV = 0.0f;
	float m_newCockpitFOV = 0.0f;
	float m_newHUDVerticalPosition = 0.0f;
	uint32_t m_newHUDWidth = 0;

	enum CameraFOVInstructionsIndex
	{
		General,
		OutsideViews,
		Cockpit
	};

	enum HUDInstructionsIndex
	{
		HUDWidth,
		HUDVertical
	};

	inline static RallyMastersFix* s_instance_ = nullptr;
};

static std::unique_ptr<RallyMastersFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<RallyMastersFix>(hModule);
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