#include "..\..\common\FixBase.hpp"

class AmericanChopperFix final : public FixBase
{
public:
	explicit AmericanChopperFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AmericanChopperFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AmericanChopperFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "American Chopper";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Default.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 8E ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 51 52 E8");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esi + 0x130);
				int& iCurrentHeight = Memory::ReadMem(ctx.esi + 0x134);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioAndCameraFOVScanResult = Memory::PatternScan(ExeModule(), "8b 42 ?? 8B 4C 24 ?? 50");
		if (AspectRatioAndCameraFOVScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioAndCameraFOVScanResult - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioAndCameraFOVScanResult + 3 - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioAndCameraFOVScanResult, 7);

			m_aspectRatioAndCameraFOVHook = safetyhook::create_mid(AspectRatioAndCameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = Memory::ReadMem(ctx.edx + 0x24);
				s_instance_->m_newAspectRatio2 = fCurrentAspectRatio / s_instance_->m_aspectRatioScale;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio2);

				float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x1C);
				s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioAndCameraFOVHook{};

	inline static AmericanChopperFix* s_instance_ = nullptr;
};

static std::unique_ptr<AmericanChopperFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AmericanChopperFix>(hModule);
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