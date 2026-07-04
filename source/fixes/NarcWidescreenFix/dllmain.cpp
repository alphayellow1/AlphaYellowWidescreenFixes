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
		return "1.3";
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
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "81 7D ?? ?? ?? ?? ?? 72 ?? 81 7D", "89 0D ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 85",
		"a1 ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 8b 4d");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 54);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.edx);
				s_instance_->m_newResY = Memory::ReadRegister(ctx.ecx);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_newAspectRatio2 = 1.0f / s_instance_->m_newAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});

			Memory::PatchBytes(ResolutionScansResult[ResIndexSelection], "\x6A\x00\x90\x90\x90\x90");
			Memory::PatchBytes(ResolutionScansResult[ResIndexSelection] + 23, "\x6A\x00\x90\x90\x90\x90\x90");
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D9 5D ?? EB ?? D9 05 ?? ?? ?? ?? D9 5D ?? 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 45 ?? D9 1C 24 A1",
		"D9 05 ?? ?? ?? ?? D9 5D ?? 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 45 ?? D9 1C 24 A1",
		"D9 05 ?? ?? ?? ?? D9 5D ?? EB ?? D9 05 ?? ?? ?? ?? D9 5D ?? 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 05 ?? ?? ?? ?? D9 1C 24 D9 05",
		"D9 05 ?? ?? ?? ?? D9 5D ?? 51 D9 05 ?? ?? ?? ?? D9 1C 24 51 D9 05 ?? ?? ?? ?? D9 1C 24 D9 05",
		"D9 05 ?? ?? ?? ?? D9 5D ?? EB ?? D9 E8 D9 5D ?? 51 D9 E8",
		"74 ?? D9 05 ?? ?? ?? ?? D9 5D ?? EB ?? D9 E8");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR5] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR6] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[AR1], 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR2], 6);

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR3], 6);

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR4], 6);

			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR5], 6);

			m_aspectRatio5Hook = safetyhook::create_mid(AspectRatioScansResult[AR5], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR6], 2);
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D9 1C 24 8B 8D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 85",
		"D9 44 10 ?? D9 5D ?? D9 45 ?? 8B E5 5D C3 CC CC CC CC 55 8B EC 83 EC ?? 89 4D ?? 8B 45 ?? 83 B8");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult))
		{
			spdlog::info("Hipfire FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon Zoom FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)ExeModule());

			m_newHipfireFOV = m_originalHipfireFOV * m_hipfireFOVFactor;

			Memory::Write(CameraFOVScansResult[Hipfire] + 2, &m_newHipfireFOV);

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 4);

			m_zoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = Memory::ReadMem(ctx.eax + ctx.edx + 0x10);
				s_instance_->m_newZoomFOV = fCurrentZoomFOV * s_instance_->m_zoomFactor;
				FPU::FLD(s_instance_->m_newZoomFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalHipfireFOV = 56.0f;

	float m_hipfireFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;

	float m_newHipfireFOV = 0.0f;
	float m_newZoomFOV = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_aspectRatio5Hook{};
	SafetyHookMid m_hipfireFOVHook{};
	SafetyHookMid m_zoomFOVHook{};

	enum ResolutionInstructionsIndices
	{
		ResListUnlock,
		ResWidthHeight,
		ResIndexSelection
	};

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2,
		AR3,
		AR4,
		AR5,
		AR6
	};

	enum CameraFOVInstructionsIndices
	{
		Hipfire,
		Zoom
	};

	inline static NarcFix* s_instance_ = nullptr;
};

static std::unique_ptr<NarcFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
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