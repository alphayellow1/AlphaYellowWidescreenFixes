#include "..\..\common\FixBase.hpp"

class BionicleFix final : public FixBase
{
public:
	explicit BionicleFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BionicleFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BionicleTheGameWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Bionicle: The Game";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "BIONICLE.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 8B 4C 24 ?? 8B 54 24 ?? 89 46 ?? 8B 44 24");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScanResult, 8); // NOP out the original instructions

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioAndCameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 03 8B 4B ?? 57");
		if (AspectRatioAndCameraFOVScanResult)
		{
			spdlog::info("Aspect Ratio & Camera FOV Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioAndCameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioAndCameraFOVScanResult, 5); // NOP out the original instructions

			m_aspectRatioAndCameraFOVHook = safetyhook::create_mid(AspectRatioAndCameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->AspectRatioAndFOVMidHook(ctx);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio & camera FOV instructions scan memory address.");
			return;
		}

		auto FrustumCullingScanResult = Memory::PatternScan(ExeModule(), "D8 3D ?? ?? ?? ?? D9 5B");
		if (FrustumCullingScanResult)
		{
			spdlog::info("Frustum Culling Instruction: Address is {:s}+{:x}", ExeName().c_str(), FrustumCullingScanResult - (std::uint8_t*)ExeModule());

			m_newFrustumCullingValue = 16.0f;

			Memory::Write(FrustumCullingScanResult + 2, &m_newFrustumCullingValue);			
		}
		else
		{
			spdlog::error("Failed to locate frustum culling instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioAndCameraFOVHook{};

	float m_newFrustumCullingValue = 0.0f;

	void AspectRatioAndFOVMidHook(SafetyHookContext& ctx)
	{
		ctx.eax = std::bit_cast<uintptr_t>(m_newAspectRatio);
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.ebx + 0x8);
		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;
		ctx.ecx = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static BionicleFix* s_instance_ = nullptr;
};

static std::unique_ptr<BionicleFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BionicleFix>(hModule);
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