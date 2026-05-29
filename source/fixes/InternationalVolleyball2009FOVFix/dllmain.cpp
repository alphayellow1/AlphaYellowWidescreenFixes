#include "..\..\common\FixBase.hpp"

class InternationalVolleyball2009Fix final : public FixBase
{
public:
	explicit InternationalVolleyball2009Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~InternationalVolleyball2009Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "InternationalVolleyball2009FOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "International Volleyball 2009";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "IV2009.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "0F 82 ?? ?? ?? ?? 8B 35", "8B 08 8B 54 24 ?? 4A");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 6);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.eax);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x4);
				s_instance_->m_newAspectRatio = static_cast<double>(iCurrentWidth) / static_cast<double>(iCurrentHeight);
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "DD 05 ?? ?? ?? ?? DD 5C 24 ?? D9 46 ?? DD 1C 24 E8 ?? ?? ?? ?? D9",
		"DC 35 ?? ?? ?? ?? D9 9B ?? ?? ?? ?? D9 46", "DC 35 ?? ?? ?? ?? D9 9B ?? ?? ?? ?? D9 05", "DD 05 ?? ?? ?? ?? DD 5C 24 ?? D9 46 ?? DD 1C 24 E8 ?? ?? ?? ?? 8B");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult, AR1, AR4, 0, 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], [](SafetyHookContext& ctx)
			{
				FPU::FDIV(s_instance_->m_newAspectRatio);
			});

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], [](SafetyHookContext& ctx)
			{
				FPU::FDIV(s_instance_->m_newAspectRatio);
			});

			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? 8D 44 24 ?? 50 D9 5C 24 ?? 8B", "D9 46 ?? D8 CA D9 46 ?? D8 CA DE C1 D9 5C 24 ?? D9 46 ?? D8");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes] + 5 - (std::uint8_t*)ExeModule());

			m_gameplayFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[Gameplay] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 6);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_gameplayFOVAddress);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes], 3);

			m_cutscenesFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0x50);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes] + 5, 3);

			m_cutscenesFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes] + 5, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0x1C);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	double m_newAspectRatio = 0.0;

	uintptr_t m_gameplayFOVAddress = 0;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_cutscenesFOV1Hook{};
	SafetyHookMid m_cutscenesFOV2Hook{};

	enum ResolutionInstructionsIndex
	{
		ResListUnlock,
		ResWidthHeight
	};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3,
		AR4
	};

	enum CameraFOVInstructionsIndex
	{
		Gameplay,
		Cutscenes
	};

	void CameraFOVMidHook(uintptr_t FOVAddress)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;

		FPU::FLD(m_newCameraFOV);
	}

	inline static InternationalVolleyball2009Fix* s_instance_ = nullptr;
};

static std::unique_ptr<InternationalVolleyball2009Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<InternationalVolleyball2009Fix>(hModule);
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