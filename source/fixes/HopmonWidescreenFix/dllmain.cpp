#include "..\..\common\FixBase.hpp"

class HopmonFix final : public FixBase
{
public:
	explicit HopmonFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~HopmonFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "HopmonWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.0";
	}

	const char* TargetName() const override
	{
		return "Hopmon";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Hopmon.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "72 ?? 8B 4D ?? 81 79", "6A ?? 8B 55 ?? 52 FF 15 ?? ?? ?? ?? EB ?? 6A");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 2);
			Memory::PatchBytes(ResolutionScansResult[ResListUnlock] + 12, "\xEB\x02");
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 40, 2);
			Memory::PatchBytes(ResolutionScansResult[ResListUnlock] + 49, "\xEB\x02");

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0x88D24);
				int& iCurrentHeight = Memory::ReadMem((uintptr_t)s_instance_->ExeModule() + 0x88D20);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "FF D2 8D 8D ?? ?? ?? ?? E8");
		auto CameraFOVScanResult = AspectRatioScanResult;

		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult + 42 - (uint8_t*)ExeModule());

			m_aspectRatioAddress = Memory::GetPointerFromAddress(AspectRatioScanResult + 46, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(AspectRatioScanResult + 42, 8);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult + 42, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = Memory::ReadMem(s_instance_->m_aspectRatioAddress);
				s_instance_->m_newAspectRatio2 = fCurrentAspectRatio / s_instance_->m_aspectRatioScale;
				ctx.xmm0.f32[0] = s_instance_->m_newAspectRatio2;
			});
		}

		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult + 56 - (uint8_t*)ExeModule());

			m_cameraFOVAddress = Memory::GetPointerFromAddress(AspectRatioScanResult + 60, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScanResult + 56, 8);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult + 56, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(s_instance_->m_cameraFOVAddress);
				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
				ctx.xmm0.f32[0] = s_instance_->m_newCameraFOV;
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	uintptr_t m_aspectRatioAddress = 0;
	uintptr_t m_cameraFOVAddress = 0;

	enum ResolutionInstructionsIndices
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum CameraFOVInstructionsScansIndices
	{
		Menu,
		Races1,
		Races2
	};

	inline static HopmonFix* s_instance_ = nullptr;
};

static std::unique_ptr<HopmonFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<HopmonFix>(hModule);
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