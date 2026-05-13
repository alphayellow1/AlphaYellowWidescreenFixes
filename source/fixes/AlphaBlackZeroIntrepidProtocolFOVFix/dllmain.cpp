#include "..\..\common\FixBase.hpp"

class AlphaBlackZeroFix final : public FixBase
{
public:
	explicit AlphaBlackZeroFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AlphaBlackZeroFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AlphaBlackZeroIntrepidProtocolFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Alpha Black Zero: Intrepid Protocol";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Abz.exe");
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
		m_dllModule2 = Memory::GetHandle("Engine.dll");
		m_dllModule3 = Memory::GetHandle("EntitiesMP.dll");
		m_dllModule2Name = Memory::GetModuleName(m_dllModule2);
		m_dllModule3Name = Memory::GetModuleName(m_dllModule3);

		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 45 ?? 8B 55 ?? 8B 4D ?? A3");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.ebp + 0x10);
				int& iCurrentHeight = Memory::ReadMem(ctx.ebp + 0x14);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(m_dllModule2, "8B 8A 94 01 00 00 89 88 94 01 00 00 8B 8A 98 01 00 00 89 88 98 01 00 00 8B 8A 9C 01 00 00 89 88 9C 01 00 00 8B 8A A0 01 00 00 89 88 A0 01 00 00 8B 8A A4 01 00 00 89 88 A4 01 00 00 8B 8A A8 01 00 00 89 88 A8 01 00 00 81 C2 AC 01 00 00 8B 32 8D 88 AC 01 00 00 89 31 8B 72 04 89 71 04 8B 72 08 89 71 08 8B 52 0C 5F 89 51 0C 5E 5D C2 04 00 90 90 90 90 90 90 90 90 90 90 90 90 90 56",
		m_dllModule3, "68 ?? ?? ?? ?? 8D B3", "C7 45 ?? ?? ?? ?? ?? EB ?? 8B CE E8 ?? ?? ?? ?? D9 5D", "68 ?? ?? ?? ?? 8D 8B ?? ?? ?? ?? E8", "A1 ?? ?? ?? ?? 89 46 ?? 5F",
		"D9 81 ?? ?? ?? ?? E8");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Overall FOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), CameraFOVScansResult[Overall] - (std::uint8_t*)m_dllModule2);
			spdlog::info("Hipfire FOV Instruction 1: Address is {:s}+{:x}", m_dllModule3Name.c_str(), CameraFOVScansResult[Hipfire1] - (std::uint8_t*)m_dllModule3);
			spdlog::info("Hipfire FOV Instruction 2: Address is {:s}+{:x}", m_dllModule3Name.c_str(), CameraFOVScansResult[Hipfire2] - (std::uint8_t*)m_dllModule3);
			spdlog::info("Hipfire FOV Instruction 3: Address is {:s}+{:x}", m_dllModule3Name.c_str(), CameraFOVScansResult[Hipfire3] - (std::uint8_t*)m_dllModule3);
			spdlog::info("Hipfire FOV Instruction 4: Address is {:s}+{:x}", m_dllModule3Name.c_str(), CameraFOVScansResult[Hipfire4] - (std::uint8_t*)m_dllModule3);
			spdlog::info("Weapon Zoom Instruction: Address is {:s}+{:x}", m_dllModule3Name.c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)m_dllModule3);

			Memory::WriteNOPs(CameraFOVScansResult[Overall], 6);

			m_overallFOVHook = safetyhook::create_mid(CameraFOVScansResult[Overall], [](SafetyHookContext& ctx)
			{
				float& fCurrentOverallFOV = Memory::ReadMem(ctx.edx + 0x194);

				if (fCurrentOverallFOV != s_instance_->m_newOverallFOV)
				{
					s_instance_->m_newOverallFOV = Maths::CalculateNewFOV_DegBased(fCurrentOverallFOV, s_instance_->m_aspectRatioScale);
				}				

				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newOverallFOV);
			});

			m_newHipfireFOV = m_originalHipfireFOV * m_hipfireFOVFactor;

			Memory::Write(CameraFOVScansResult[Hipfire1] + 1, m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[Hipfire2] + 3, m_newHipfireFOV);
			Memory::Write(CameraFOVScansResult[Hipfire3] + 1, m_newHipfireFOV);

			HipfireFOV4Address = Memory::GetPointerFromAddress(CameraFOVScansResult[Hipfire4] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[Hipfire4], 5);

			m_hipfireFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[Hipfire4], [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireFOV4 = Memory::ReadMem(s_instance_->HipfireFOV4Address);

				s_instance_->m_newHipfireFOV4 = fCurrentHipfireFOV4 * s_instance_->m_hipfireFOVFactor;

				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newHipfireFOV4);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 6);

			m_zoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = Memory::ReadMem(ctx.ecx + 0xD8);

				s_instance_->m_newZoomFOV = fCurrentZoomFOV / s_instance_->m_zoomFactor;

				FPU::FLD(s_instance_->m_newZoomFOV);
			});
		}
	}

private:
	HMODULE m_dllModule2 = nullptr;
	HMODULE m_dllModule3 = nullptr;
	std::string m_dllModule2Name;
	std::string m_dllModule3Name;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_overallFOVHook{};
	SafetyHookMid m_hipfireFOV4Hook{};
	SafetyHookMid m_zoomFOVHook{};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	static constexpr float m_originalHipfireFOV = 78.0f;
	uintptr_t HipfireFOV4Address = 0;
	float m_newOverallFOV = 0.0f;
	float m_newHipfireFOV = 0.0f;
	float m_newHipfireFOV4 = 0.0f;
	float m_newZoomFOV = 0.0f;

	float m_hipfireFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;

	enum CameraFOVInstructionsIndices
	{
		Overall,
		Hipfire1,
		Hipfire2,
		Hipfire3,
		Hipfire4,
		Zoom
	};	

	inline static AlphaBlackZeroFix* s_instance_ = nullptr;
};

static std::unique_ptr<AlphaBlackZeroFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AlphaBlackZeroFix>(hModule);
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