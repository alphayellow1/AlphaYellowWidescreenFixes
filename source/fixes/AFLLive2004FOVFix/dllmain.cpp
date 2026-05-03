#include "..\..\common\FixBase.hpp"

class AFLLive2004Fix final : public FixBase
{
public:
	explicit AFLLive2004Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AFLLive2004Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AFLLive2004FOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "AFL Live 2004";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AFL2004.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 02 A3 ?? ?? ?? ?? 8B 42");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_ResolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);

				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;

				spdlog::info("Current Resolution: {}x{}", iCurrentWidth, iCurrentHeight);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVInstructionsScanResult = Memory::PatternScan(ExeModule(), "8B 45 ?? 5F 89 45");
		if (CameraFOVInstructionsScanResult)
		{
			spdlog::info("Camera FOV Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionsScanResult, 3);

			m_CameraHFOVHook = safetyhook::create_mid(CameraFOVInstructionsScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x1C, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor, ECX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult + 10, 3);

			m_CameraVFOVHook = safetyhook::create_mid(CameraFOVInstructionsScanResult + 10, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0x2E0, 1.0f / s_instance_->m_aspectRatioScale, 1.0f / s_instance_->m_fovFactor, FDIV, ctx);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;

	SafetyHookMid m_ResolutionHook{};
	SafetyHookMid m_CameraHFOVHook{};
	SafetyHookMid m_CameraVFOVHook{};

	enum CameraFOVInstructionsIndices
	{
		HFOV,
		VFOV
	};

	enum DestInstruction
	{
		FDIV,
		ECX
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, float arScale, float fovFactor, DestInstruction destInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = fCurrentCameraFOV * arScale * fovFactor;

		switch (destInstruction)
		{
			case FDIV:
			{
				FPU::FDIV(m_newCameraFOV);
				break;
			}

			case ECX:
			{
				ctx.ecx = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}
		}
	}

	inline static AFLLive2004Fix* s_instance_ = nullptr;
};

static std::unique_ptr<AFLLive2004Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AFLLive2004Fix>(hModule);
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