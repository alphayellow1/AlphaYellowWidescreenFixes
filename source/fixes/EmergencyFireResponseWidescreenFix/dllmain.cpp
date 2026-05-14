#include "..\..\common\FixBase.hpp"

class EmergencyFireResponseFix final : public FixBase
{
public:
	explicit EmergencyFireResponseFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~EmergencyFireResponseFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EmergencyFireResponseWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Secret Service";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "FDMASTER.EXE");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "MainMenuWidth", m_newMenuWidth);
		inipp::get_value(ini.sections["Settings"], "MainMenuHeight", m_newMenuHeight);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_newMenuWidth);
		spdlog_confparse(m_newMenuHeight);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "81 FA ?? ?? ?? ?? 72 ?? 8B 4E", "8B 75 ?? 57 8B 7D ?? 53",
		"C7 45 ?? ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? 53", "BE ?? ?? ?? ?? BF ?? ?? ?? ?? 56 57", "BF ?? ?? ?? ?? 6A ?? BE ?? ?? ?? ?? 57 56 E8");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[MainMenu1] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[MainMenu2] - (std::uint8_t*)ExeModule());
			spdlog::info("Loading Screen Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[LoadingScreen] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 6, 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 17, 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 50, 2);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
					const int iCurrentWidth = Memory::ReadMem(ctx.ebp + 0x8);
					const int iCurrentHeight = Memory::ReadMem(ctx.ebp + 0xC);

					const float newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);

					const bool aspectChanged = !s_instance_->m_hasResolution || std::abs(s_instance_->m_newAspectRatio - newAspectRatio) >= 0.0001f;

					s_instance_->m_newAspectRatio = newAspectRatio;
					s_instance_->m_hasResolution = true;

					const bool ar3NeedsUpdate = s_instance_->m_ar3HasContext && std::abs(s_instance_->m_ar3AppliedAspectRatio - newAspectRatio) >= 0.0001f;

					if (!aspectChanged && !ar3NeedsUpdate)
					{
						return;
					}

					if (!s_instance_->m_ar3HasContext)
					{
						return;
					}

					if (s_instance_->m_ar3This == 0 || s_instance_->m_ar3CallTarget == 0)
					{
						return;
					}

					if (s_instance_->m_callingAR3Function)
					{
						return;
					}

					s_instance_->m_callingAR3Function = true;

					auto function = reinterpret_cast<AR3AspectFunction>(s_instance_->m_ar3CallTarget);

					const float ar3Arg1 = Memory::ReadMem(s_instance_->m_ar3This + 0xA4);
					const float ar3Arg3 = Memory::ReadMem(s_instance_->m_ar3This + 0xAC);
					const float ar3Arg4 = Memory::ReadMem(s_instance_->m_ar3This + 0xB0);

					function(reinterpret_cast<void*>(s_instance_->m_ar3This), ar3Arg1, s_instance_->m_newAspectRatio, ar3Arg3, ar3Arg4);

					s_instance_->m_ar3AppliedAspectRatio = s_instance_->m_newAspectRatio;
					s_instance_->m_callingAR3Function = false;
			});

			Memory::Write(ResolutionScansResult[MainMenu1] + 3, m_newMenuWidth);
			Memory::Write(ResolutionScansResult[MainMenu1] + 10, m_newMenuHeight);
			Memory::Write(ResolutionScansResult[MainMenu2] + 6, m_newMenuWidth);
			Memory::Write(ResolutionScansResult[MainMenu2] + 1, m_newMenuHeight);
			Memory::Write(ResolutionScansResult[LoadingScreen] + 8, m_newMenuWidth);
			Memory::Write(ResolutionScansResult[LoadingScreen] + 1, m_newMenuHeight);
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? 51 D9 1C ?? D9 86 ?? ?? ?? ?? 51 8B CE D9 1C ?? E8 ?? ?? ?? ?? 5E C9 C3 55",
		"D9 05 ?? ?? ?? ?? 51 D9 1C ?? D9 86 ?? ?? ?? ?? 51 8B CE D9 1C ?? E8 ?? ?? ?? ?? 5E C9 C3 D9 44 24", "D9 05 ?? ?? ?? ?? 51 D9 1C ?? D9 45 ?? 51 8B CE",
		"D9 05 ?? ?? ?? ?? 51 D9 1C ?? D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 51 D9 1C ?? E8 ?? ?? ?? ?? D9 EE 8D 45 ?? 68 ?? ?? ?? ?? D9 5D ?? D9 EE 50 8D 45 ?? D9 5D ?? D9 EE 50 8B CE D9 5D ?? D9 05 ?? ?? ?? ?? D9 55 ?? D9 55 ?? D9 5D ?? E8 ?? ?? ?? ?? 5E C9 C2");
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
				FPU::FLD(s_instance_->m_newAspectRatio);
			});

			m_ar3InstructionAddress = reinterpret_cast<uintptr_t>(AspectRatioScansResult[AR3]);

			const uintptr_t ar3CallInstruction = m_ar3InstructionAddress + 19;

			if (*reinterpret_cast<uint8_t*>(ar3CallInstruction) == 0xE8)
			{
				m_ar3CallTarget = GetRelativeCallTarget(ar3CallInstruction);
			}
			else
			{
				spdlog::error("AR3 call instruction was not found at expected offset. Byte at +19: {:02X}", *reinterpret_cast<uint8_t*>(ar3CallInstruction));
			}

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], [](SafetyHookContext& ctx)
			{
				s_instance_->m_ar3This = ctx.esi;
				s_instance_->m_ar3HasContext = true;

				FPU::FLD(s_instance_->m_newAspectRatio);

				s_instance_->m_ar3AppliedAspectRatio = s_instance_->m_newAspectRatio;
			});

			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 45 ?? 51 8B CE D9 1C ?? E8 ?? ?? ?? ?? 5E C9 C2 ?? ?? 55 8B EC 83 EC ?? A1");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 3);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.ebp + 0xC);

				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;

				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_cameraFOVHook{};

	uint32_t m_newMenuWidth = 0;
	uint32_t m_newMenuHeight = 0;

	float m_ar3AppliedAspectRatio = 0.0f;
	bool m_ar3NeedsReapply = false;

	float m_newAspectRatio = m_oldAspectRatio;
	bool m_hasResolution = false;

	uintptr_t m_ar3InstructionAddress = 0;
	uintptr_t m_ar3CallTarget = 0;
	uintptr_t m_ar3This = 0;

	float m_ar3Arg1 = 0.0f;
	float m_ar3Arg3 = 0.0f;
	float m_ar3Arg4 = 0.0f;

	bool m_ar3HasContext = false;
	bool m_callingAR3Function = false;

	using AR3AspectFunction = void(__thiscall*)(void* thisptr, float arg1, float aspect, float arg3, float arg4);

	enum ResolutionInstructionsIndex
	{
		ResListUnlock,
		ResWidthHeight,
		MainMenu1,
		MainMenu2,
		LoadingScreen
	};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3,
		AR4
	};

	static uintptr_t GetRelativeCallTarget(uintptr_t callInstruction)
	{
		const auto rel = *reinterpret_cast<int32_t*>(callInstruction + 1);
		return callInstruction + 5 + rel;
	}

	inline static EmergencyFireResponseFix* s_instance_ = nullptr;
};

static std::unique_ptr<EmergencyFireResponseFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<EmergencyFireResponseFix>(hModule);
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