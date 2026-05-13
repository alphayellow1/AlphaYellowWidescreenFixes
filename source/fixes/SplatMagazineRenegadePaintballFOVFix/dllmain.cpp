#include "..\..\common\FixBase.hpp"

class RenegadePaintballFix final : public FixBase
{
public:
	explicit RenegadePaintballFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RenegadePaintballFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SplatMagazineRenegadePaintballFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Splat Magazine Renegade Paintball";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "PaintballGame.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "HipfireFOVFactor", m_hipfireFOVFactor);
		inipp::get_value(ini.sections["Settings"], "WeaponFOVFactor", m_weaponFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		spdlog_confparse(m_hipfireFOVFactor);
		spdlog_confparse(m_weaponFOVFactor);
		spdlog_confparse(m_zoomFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "6A ?? 89 74 24 ?? FF 15", "8B 8C 24 ?? ?? ?? ?? 8B BC 24 ?? ?? ?? ?? 89 0E");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 303);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x148);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x14C);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;

				s_instance_->WriteAR();
			});
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "C7 40 ?? ?? ?? ?? ?? DD 05 ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? D9 F2 50 DD D8 D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FE D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FF D9 98 ?? ?? ?? ?? C7 40",
		"C7 43 ?? ?? ?? ?? ?? 8B 80", "C7 42 ?? ?? ?? ?? ?? 8B 06 8B 54 24", "C7 41 ?? ?? ?? ?? ?? EB ?? A1", "C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? C6 84 24 ?? ?? ?? ?? ?? C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? C7 84 24 ?? ?? ?? ?? ?? ?? ?? ?? E8",
		"C7 40 ?? ?? ?? ?? ?? DD 05 ?? ?? ?? ?? C7 40 ?? ?? ?? ?? ?? D9 F2 50 DD D8 D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FE D9 98 ?? ?? ?? ?? DD 05 ?? ?? ?? ?? D9 FF D9 98 ?? ?? ?? ?? 8B C5", "C7 41 ?? ?? ?? ?? ?? 8B 55 ?? 8D 86", "C7 40 ?? ?? ?? ?? ?? 8B 45 ?? 8B 54 24",
		"C7 43 ?? ?? ?? ?? ?? D8 0D", "C7 42 ?? ?? ?? ?? ?? 8B C8");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR5] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR6] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR7] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR8] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR9] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 10: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR10] - (std::uint8_t*)ExeModule());
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 40 ?? 8B 86 ?? ?? ?? ?? D9 50", "D9 83 ?? ?? ?? ?? 8D 44 24 ?? D9 83 ?? ?? ?? ?? 50",
		"D9 83 ?? ?? ?? ?? 50", "D9 05 ?? ?? ?? ?? D8 64 24 ?? 5F", "8B 48 ?? 33 C0 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 89 44 24 ?? 8B 44 F2");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Briefing Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Briefing] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire Camera FOV Instruction 1: Address is{:s} + {:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire1] - (std::uint8_t*)ExeModule());
			spdlog::info("Hipfire Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Zoom FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)ExeModule());
			spdlog::info("Weapon FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Weapon] - (std::uint8_t*)ExeModule());

			// Briefing Camera FOV
			Memory::WriteNOPs(CameraFOVScansResult[Briefing], 3);

			m_briefingFOVHook = safetyhook::create_mid(CameraFOVScansResult[Briefing], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.eax + 0x4, 1.0f, FLD, ctx);
			});

			// Hipfire Camera FOV 1
			Memory::WriteNOPs(CameraFOVScansResult[Hipfire1], 6);

			m_hipfireFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Hipfire1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebx + 0x19C, s_instance_->m_hipfireFOVFactor, FLD, ctx);
			});

			// Hipfire Camera FOV 2
			Memory::WriteNOPs(CameraFOVScansResult[Hipfire2], 6);

			m_hipfireFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Hipfire2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebx + 0x19C, s_instance_->m_hipfireFOVFactor, FLD, ctx);
			});

			// Camera Zoom FOV
			m_cameraZoomAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[Zoom] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 6);

			m_ZoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraZoomAddress, 1.0f / s_instance_->m_zoomFactor, FLD, ctx);
			});

			// Weapon FOV
			Memory::WriteNOPs(CameraFOVScansResult[Weapon], 3);

			m_weaponFOVHook = safetyhook::create_mid(CameraFOVScansResult[Weapon], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.eax + 0x4, s_instance_->m_weaponFOVFactor, ECX, ctx);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	std::vector<std::uint8_t*> AspectRatioScansResult;

	float m_hipfireFOVFactor = 0.0f;
	float m_weaponFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;

	uintptr_t m_cameraZoomAddress = 0;

	SafetyHookMid m_resolutionHook{};	
	SafetyHookMid m_briefingFOVHook{};
	SafetyHookMid m_hipfireFOV1Hook{};
	SafetyHookMid m_hipfireFOV2Hook{};
	SafetyHookMid m_ZoomFOVHook{};
	SafetyHookMid m_weaponFOVHook{};

	enum DestInstruction
	{
		FLD,
		ECX
	};

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
		AR4,
		AR5,
		AR6,
		AR7,
		AR8,
		AR9,
		AR10
	};

	enum CameraFOVInstructionsIndex
	{
		Briefing,
		Hipfire1,
		Hipfire2,
		Zoom,
		Weapon
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, float fovFactor, DestInstruction destInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;

		switch (destInstruction)
		{
			case FLD:
			{
				FPU::FLD(m_newCameraFOV);
				break;
			}

			case ECX:
			{
				ctx.ecx = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}
		}
	}
	
	void WriteAR()
	{
		Memory::Write(AspectRatioScansResult, AR1, AR4, 3, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR5] + 7, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult, AR6, AR10, 3, m_newAspectRatio);
	}

	inline static RenegadePaintballFix* s_instance_ = nullptr;
};

static std::unique_ptr<RenegadePaintballFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<RenegadePaintballFix>(hModule);
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