#include "..\..\common\FixBase.hpp"

class Anubis2Fix final : public FixBase
{
public:
	explicit Anubis2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~Anubis2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AnubisIIFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Anubis II";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Anubis In Egypt II.exe");
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

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.edx);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}
		
		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 76 ?? DB 05");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 3);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = Memory::ReadMem(ctx.esi + 0x14);
				s_instance_->m_newAspectRatio2 = fCurrentAspectRatio / s_instance_->m_aspectRatioScale;
				FPU::FDIV(s_instance_->m_newAspectRatio2);
			});
		}
		else
		{
			spdlog::info("Failed to locate the aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 55 ?? 48 52 50 E8 ?? ?? ?? ?? 8B 4D", "8B 45 ?? 49 50");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, FOV1, FOV2, 0, 3);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebp + 0x74, ctx.edx);
			});

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebp + 0x78, ctx.eax);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2
	};

	void CameraFOVMidHook(uintptr_t SourceAddr, uintptr_t& DestAddr)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(SourceAddr);
		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;
		DestAddr = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static Anubis2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<Anubis2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<Anubis2Fix>(hModule);
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