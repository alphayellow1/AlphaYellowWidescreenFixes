#include "..\..\common\FixBase.hpp"

class DW4Fix final : public FixBase
{
public:
	explicit DW4Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DW4Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DynastyWarriors4WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Dynasty Warriors 4";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Dynasty Warriors 4 Hyper.exe") ||
		Util::stringcmp_caseless(exeName, "Shin Sangokumusou 3.exe") ||
		// Util::stringcmp_caseless(exeName, "Jin Samgukmussang 3 Hyper.exe") ||
		// Util::stringcmp_caseless(exeName, "Zhen SanGuoWuShuang 3 Hyper.exe") ||
		Util::stringcmp_caseless(exeName, "DW4Config.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "DW4Config.exe"))
		{
			auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "0F 82 ?? ?? ?? ?? 8B 44 24");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionScanResult, 6);
				Memory::WriteNOPs(ResolutionScanResult + 15, 6);
				Memory::WriteNOPs(ResolutionScanResult + 33, 2);
				Memory::WriteNOPs(ResolutionScanResult + 47, 2);
			}
			else
			{
				spdlog::info("Failed to locate the resolution list unlock scan memory address.");
				return;
			}
		}
		
		if (Util::stringcmp_caseless(ExeName(), "Dynasty Warriors 4 Hyper.exe") || Util::stringcmp_caseless(ExeName(), "Shin Sangokumusou 3.exe"))
		{
			auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 54 24 ?? 57 8B 7C 24 ?? 89 50", "0F 82 ?? ?? ?? ?? 8B 53");
			if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());

				m_resolutionWidthHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight] + 5, [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentWidth = Memory::ReadMem(ctx.esp + 0x28);

					s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
				});

				m_resolutionHeightHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentHeight = Memory::ReadMem(ctx.esp + 0x28);
				});

				Memory::WriteNOPs(ResolutionScansResult[ResListUnlock], 6);
				Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 15, 6);
				Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 33, 2);
				Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 47, 2);
			}

			auto CharactersNameAspectRatioScanResult = Memory::PatternScan(ExeModule(), "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 44 24 ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB ?? 83 FF ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB ?? 83 FF ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB ?? 83 FF ?? 75 ?? F3 0F 10 05 ?? ?? ?? ?? EB");
			if (CharactersNameAspectRatioScanResult)
			{
				spdlog::info("Characters Name Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), CharactersNameAspectRatioScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CharactersNameAspectRatioScanResult, 8); // NOP out the original instruction

				m_charactersNameAspectRatioHook = safetyhook::create_mid(CharactersNameAspectRatioScanResult, [](SafetyHookContext& ctx)
					{
						ctx.xmm0.f32[0] = s_instance_->m_newAspectRatio;
					});
			}
			else
			{
				spdlog::error("Failed to locate character name aspect ratio instruction memory address.");
				return;
			}

			auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "F3 0F 11 9E ?? ?? ?? ?? F3 0F 10 1D ?? ?? ?? ?? F3 0F 11 9E ?? ?? ?? ?? F3 0F 10 1D ?? ?? ?? ?? F3 0F 11 9E ?? ?? ?? ?? F3 0F 10 1D ?? ?? ?? ?? F3 0F 11 9E ?? ?? ?? ??",
			"F3 0F 11 85 ?? ?? ?? ?? 8B 17 89 10 8B 4F ??", "F3 0F 58 05 ?? ?? ?? ?? F3 0F 11 87 ?? ?? ?? ??", "C3 CC CC 8B 44 24 ?? F3 0F 10 44 24 ?? 8D 04 40 C1 E0 ?? F3 0F 11 80 ?? ?? ?? ?? C3 CC CC CC");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Main Menu Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[MainMenu] - (std::uint8_t*)ExeModule());

				spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay1] - (std::uint8_t*)ExeModule());

				spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay2] + 8 - (std::uint8_t*)ExeModule());

				spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes] + 19 - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVScansResult[MainMenu], 8);

				m_mainMenuFOVHook = safetyhook::create_mid(CameraFOVScansResult[MainMenu], [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVInstructionsMidHook(ctx.xmm3.f32[0], ctx.esi + 0x98, FullAngle);
				});

				Memory::WriteNOPs(CameraFOVScansResult[Gameplay1], 8);

				m_gameplayFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay1], [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], ctx.ebp + 0x98, FullAngle, s_instance_->m_fovFactor);
				});

				Memory::WriteNOPs(CameraFOVScansResult[Gameplay2] + 8, 8);

				m_gameplayFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay2] + 8, [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], ctx.edi + 0x98, FullAngle, s_instance_->m_fovFactor);
				});

				m_cutscenesFOVOffset = Memory::GetPointerFromAddress(CameraFOVScansResult[Cutscenes] + 23, Memory::PointerMode::Absolute);

				Memory::WriteNOPs(CameraFOVScansResult[Cutscenes] + 19, 8);

				m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes] + 19, [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVInstructionsMidHook(ctx.xmm0.f32[0], ctx.eax + s_instance_->m_cutscenesFOVOffset, HalfAngle);
				});
			}
		}		
	}

private:
	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_charactersNameAspectRatioHook{};
	SafetyHookMid m_mainMenuFOVHook{};
	SafetyHookMid m_gameplayFOV1Hook{};
	SafetyHookMid m_gameplayFOV2Hook{};
	SafetyHookMid m_cutscenesFOVHook{};

	enum ResolutionInstructionsIndex
	{
		ResWidthHeight,
		ResListUnlock
	};

	enum CameraFOVInstructionsIndex
	{
		MainMenu,
		Gameplay1,
		Gameplay2,
		Cutscenes
	};

	uintptr_t m_cutscenesFOVOffset = 0;

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	int m_currentWidth = 0;
	int m_currentHeight = 0;

	enum angleMode
	{
		FullAngle,
		HalfAngle
	};

	void CameraFOVInstructionsMidHook(float& SourceAddress, uintptr_t DestAddress, angleMode AngleMode, float fovFactor = 1.0f)
	{
		float& fCurrentCameraFOV = SourceAddress;

		if (AngleMode == FullAngle)
		{
			m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;
		}
		else
		{
			m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale, Maths::AngleMode::HalfAngle) * fovFactor;
		}

		*reinterpret_cast<float*>(DestAddress) = m_newCameraFOV;
	}

	inline static DW4Fix* s_instance_ = nullptr;
};

static std::unique_ptr<DW4Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<DW4Fix>(hModule);
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