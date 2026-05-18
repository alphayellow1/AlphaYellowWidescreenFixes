#include "..\..\common\FixBase.hpp"

class Americas10MostWantedFix final : public FixBase
{
public:
	explicit Americas10MostWantedFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~Americas10MostWantedFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Americas10MostWantedFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "America's 10 Most Wanted";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "A10MW.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "HipfireFOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_zoomFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8b 45 ?? a3 ?? ?? ?? ?? 8b 45 ?? a3 ?? ?? ?? ?? 5f 5e 5b 8b e5 5d c3 55 8b ec 83 ec ?? 53 56 57 0f bf 45");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.ebp + 0x8);
				int& iCurrentHeight = Memory::ReadMem(ctx.ebp + 0xC);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? 5F 5E 5B 8B E5 5D C3 CC CC CC CC CC CC CC CC CC CC 55 8B EC 83 EC ?? 53 56 57 C7 45 ?? ?? ?? ?? ?? 8B 45");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio2 = 0.75f / s_instance_->m_aspectRatioScale;

				FPU::FLD(s_instance_->m_newAspectRatio2);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "A1 ?? ?? ?? ?? A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 50 8B 0D",
		"8B 88 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? A1 ?? ?? ?? ?? 50");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Hipfire FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire] - (std::uint8_t*)ExeModule());
			spdlog::info("Aim Zoom FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)ExeModule());

			CameraFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[Hipfire] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[Hipfire], 5);

			m_hipfireFOVook = safetyhook::create_mid(CameraFOVScansResult[Hipfire], [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireFOV = Memory::ReadMem(s_instance_->CameraFOVAddress);
				s_instance_->m_newHipfireFOV = fCurrentHipfireFOV * s_instance_->m_fovFactor;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newHipfireFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 6);

			m_zoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.eax + s_instance_->CameraFOVAddress);
				s_instance_->m_newZoomFOV = fCurrentCameraFOV2 / s_instance_->m_zoomFactor;
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newZoomFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_hipfireFOVook{};
	SafetyHookMid m_zoomFOVHook{};

	float m_zoomFactor = 0.0f;

	float m_newHipfireFOV = 0.0f;
	float m_newZoomFOV = 0.0f;

	enum CameraFOVInstructionsIndex
	{
		Hipfire,
		Zoom
	};

	uintptr_t CameraFOVAddress = 0;

	inline static Americas10MostWantedFix* s_instance_ = nullptr;
};

static std::unique_ptr<Americas10MostWantedFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<Americas10MostWantedFix>(hModule);
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