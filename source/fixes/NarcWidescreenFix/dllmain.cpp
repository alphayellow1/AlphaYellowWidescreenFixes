#include "..\..\common\FixBase.hpp"

class NarcFix final : public FixBase
{
public:
	explicit NarcFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~NarcFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "NarcWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Narc";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "charlie.exe");
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
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "81 7D ?? ?? ?? ?? ?? 72 ?? 81 7D", "89 0D ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 85");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 54);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				const int& iCurrentResX = Memory::ReadRegister(ctx.edx);

				const int& iCurrentResY = Memory::ReadRegister(ctx.ecx);

				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentResX) / static_cast<float>(iCurrentResY);

				s_instance_->m_resolutionHook.disable();
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D9 5D ?? EB ?? D9 05 ?? ?? ?? ?? D9 5D ?? 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 45 ?? D9 1C 24 A1");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult + 11 - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});

			Memory::WriteNOPs(AspectRatioScanResult + 11, 6);

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScanResult + 11, [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D9 1C 24 8B 8D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 85",
		"D9 44 10 ?? D9 5D ?? D9 45 ?? 8B E5 5D C3 CC CC CC CC 55 8B EC 83 EC ?? 89 4D ?? 8B 45 ?? 83 B8", "D9 05 ?? ?? ?? ?? D9 1C 24 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? 8B E5 5D C3 CC CC CC CC CC CC CC CC 55");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult))
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera HFOV Culling Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[HFOVCulling] - (std::uint8_t*)ExeModule());

			m_newHipfireFOV = m_originalHipfireFOV * m_hipfireFOVFactor;

			Memory::Write(CameraFOVScansResult[Hipfire] + 2, &m_newHipfireFOV);

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 4);

			m_zoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = Memory::ReadMem(ctx.eax + ctx.edx + 0x10);

				s_instance_->m_newZoomFOV = fCurrentZoomFOV * s_instance_->m_zoomFactor;

				FPU::FLD(s_instance_->m_newZoomFOV);
			});

			// This last hook is still experimental, as I want to fix the culling in a more permanent way
			m_HFOVCullingAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[HFOVCulling] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[HFOVCulling], 6);

			m_HFOVCullingHook = safetyhook::create_mid(CameraFOVScansResult[HFOVCulling], [](SafetyHookContext& ctx)
			{
				float& fCurrentHFOVCulling = Memory::ReadMem(s_instance_->m_HFOVCullingAddress);

				s_instance_->m_newHFOVCulling = fCurrentHFOVCulling / s_instance_->m_aspectRatioScale;

				FPU::FLD(s_instance_->m_newHFOVCulling);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;

	static constexpr float m_originalHipfireFOV = 56.0f;

	float m_hipfireFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;

	float m_newHipfireFOV = 0.0f;
	float m_newZoomFOV = 0.0f;
	float m_newHFOVCulling = 0.0f;

	uintptr_t m_HFOVCullingAddress = 0;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_hipfireFOVHook{};
	SafetyHookMid m_zoomFOVHook{};
	SafetyHookMid m_HFOVCullingHook{};

	enum ResolutionInstructionsIndices
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum CameraFOVInstructionsIndices
	{
		Hipfire,
		Zoom,
		HFOVCulling
	};

	inline static NarcFix* s_instance_ = nullptr;
};

static std::unique_ptr<NarcFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<NarcFix>(hModule);
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