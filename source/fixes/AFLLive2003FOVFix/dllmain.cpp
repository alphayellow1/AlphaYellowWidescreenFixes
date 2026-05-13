#include "..\..\common\FixBase.hpp"

class AFLLive2003Fix final : public FixBase
{
public:
	explicit AFLLive2003Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AFLLive2003Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AFLLive2003FOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "AFL Live 2003";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AFL2003.exe");
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
				s_instance_->CameraFOVMidHook(EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVInstructionsScanResult + 10, 3);

			m_CameraVFOVHook = safetyhook::create_mid(CameraFOVInstructionsScanResult + 10, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(FLD, ctx);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

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
		FLD,
		EAX
	};

	void CameraFOVMidHook(DestInstruction destInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.ebp + 0x10);

		m_newCameraFOV = fCurrentCameraFOV * m_aspectRatioScale * m_fovFactor;

		switch (destInstruction)
		{
			case FLD:
			{
				FPU::FLD(m_newCameraFOV);
				break;
			}

			case EAX:
			 {
				ctx.eax = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			 }
		}
	}

	inline static AFLLive2003Fix* s_instance_ = nullptr;
};

static std::unique_ptr<AFLLive2003Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AFLLive2003Fix>(hModule);
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