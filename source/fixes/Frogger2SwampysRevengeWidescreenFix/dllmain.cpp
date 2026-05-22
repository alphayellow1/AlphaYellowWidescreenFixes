#include "..\..\common\FixBase.hpp"

class Frogger2Fix final : public FixBase
{
public:
	explicit Frogger2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~Frogger2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Frogger2SwampysRevengeWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Frogger 2: Swampy's Revenge";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Frogger2(1).exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "BB ?? ?? ?? ?? 8B 16", "8B 8C 24 ?? ?? ?? ?? 8B 94 24");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(ResolutionScansResult[ResListUnlock] + 1, "\x01");
			Memory::PatchBytes(ResolutionScansResult[ResListUnlock] + 57, "\x00");
			
			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x88);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8C);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? D9 1D");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			CameraFOVAddress = Memory::GetPointerFromAddress(CameraFOVScanResult + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScanResult, 10);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraFOV = (uint32_t)((m_originalCameraFOV / s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor);

				Memory::Write(s_instance_->CameraFOVAddress, s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr uint32_t m_originalCameraFOV = 450;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	uintptr_t CameraFOVAddress = 0;
	uint32_t m_newCameraFOV = 0;

	enum ResolutionListsIndices
	{
		ResListUnlock,
		ResWidthHeight
	};	

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x30);
		m_newCameraFOV = fCurrentCameraFOV * m_aspectRatioScale * m_fovFactor;
		ctx.edx = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static Frogger2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<Frogger2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<Frogger2Fix>(hModule);
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