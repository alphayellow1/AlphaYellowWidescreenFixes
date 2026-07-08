#include "..\..\common\FixBase.hpp"

class PowerdromeFix final : public FixBase
{
public:
	explicit PowerdromeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PowerdromeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "PowerdromeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Powerdrome";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Flux.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_powerdromeDllModule = Memory::GetHandle("Powerdrome.dll");
		m_powerdromeDllModuleName = Memory::GetModuleName(m_powerdromeDllModule);

		auto ResolutionScanResult = Memory::PatternScan(m_powerdromeDllModule, "89 0D ?? ?? ?? ?? A3 ?? ?? ?? ?? D1 E9");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), ResolutionScanResult - (std::uint8_t*)m_powerdromeDllModule);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.ecx);
				s_instance_->m_newResY = Memory::ReadRegister(ctx.eax);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::info("Failed to locate the resolution instructions scan memory address.");
			return;
		}

		// These instructions are located in the following functions: piCameraItem::FactorGlanceIntoMatrix, pcTrackDollyCam::StaticClass, pcGeneratedCam::MakeCamera, pcFixedCam::MakeCamera, pcDollyCam::MakeCamera, pcCutsceneCam::StaticClass, pcCameraItem::StaticClass
		auto CameraFOVScansResult = Memory::PatternScan(m_powerdromeDllModule, "8B 54 24 ?? D8 07", "8B 44 24 ?? D8 44 24 ?? 50", "8B 4C 24 ?? D8 44 24 ?? 51", "8B 4D ?? 51 8B 4C 24", "D8 05 ?? ?? ?? ?? D9 1C ?? 51 8B 8C 24",
		"8B 44 24 ?? 50 8D 4C 24 ?? 51 8B 8C 24 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 FF 15 ?? ?? ?? ?? 81 C4 ?? ?? ?? ?? C2 ?? ?? D9 44 24 ?? D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? DD D8 C7 44 24 ?? ?? ?? ?? ?? EB ?? 90 90 90 90 90 90 90 90 90 90 83 EC",
		"8B 44 24 ?? 50 8D 4C 24 ?? 51 8B 8C 24 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 FF 15 ?? ?? ?? ?? 81 C4 ?? ?? ?? ?? C2 ?? ?? D9 44 24 ?? D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? DD D8 C7 44 24 ?? ?? ?? ?? ?? EB ?? 90 90 90 90 90 90 90 90 90 90 90",
		"8B 44 24 ?? 50 8D 4C 24 ?? 51 8B 8C 24 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 FF 15 ?? ?? ?? ?? 81 C4 ?? ?? ?? ?? C2 ?? ?? D9 44 24 ?? D9 44 24 ?? D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? DD D8 C7 44 24 ?? ?? ?? ?? ?? EB ?? 90 90 90 90 90 90 90 90 90 90 A1");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Main Menu Camera FOV Instruction: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[MainMenu] - (std::uint8_t*)m_powerdromeDllModule);
			spdlog::info("Cutscenes Camera FOV Instruction 1: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[Cutscenes1] - (std::uint8_t*)m_powerdromeDllModule);
			spdlog::info("Cutscenes Camera FOV Instruction 2: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[Cutscenes2] - (std::uint8_t*)m_powerdromeDllModule);
			spdlog::info("Cutscenes Camera FOV Instruction 3: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[Cutscenes3] - (std::uint8_t*)m_powerdromeDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[Gameplay1] - (std::uint8_t*)m_powerdromeDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[Gameplay2] - (std::uint8_t*)m_powerdromeDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 3: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[Gameplay3] - (std::uint8_t*)m_powerdromeDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 4: Address is {:s}+{:x}", m_powerdromeDllModuleName.c_str(), CameraFOVScansResult[Gameplay4] - (std::uint8_t*)m_powerdromeDllModule);

			Memory::WriteNOPs(CameraFOVScansResult[MainMenu], 4);

			m_mainMenuFOVHook = safetyhook::create_mid(CameraFOVScansResult[MainMenu], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x54, 1.0f, EDX, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes1], 4);

			m_cutscenesFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x2C, 1.0f, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes2], 4);

			m_cutscenesFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x4, 1.0f, ECX, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes3], 3);

			m_cutscenesFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebp + 0x34, 1.0f, ECX, ctx);
			});

			m_gameplayFOV1Address = Memory::GetPointerFromAddress(CameraFOVScansResult[Gameplay1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay1], 6);

			m_gameplayFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_gameplayFOV1Address, s_instance_->m_fovFactor, FADD, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay2], 4);

			m_gameplayFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x14, s_instance_->m_fovFactor, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay3], 4);

			m_gameplayFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x4, s_instance_->m_fovFactor, EAX, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay4], 4);

			m_gameplayFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay4], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x10, s_instance_->m_fovFactor, EAX, ctx);
			});
		}
	}

private:
	HMODULE m_powerdromeDllModule = nullptr;
	std::string m_powerdromeDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_mainMenuFOVHook{};
	SafetyHookMid m_cutscenesFOV1Hook{};
	SafetyHookMid m_cutscenesFOV2Hook{};
	SafetyHookMid m_cutscenesFOV3Hook{};
	SafetyHookMid m_gameplayFOV1Hook{};
	SafetyHookMid m_gameplayFOV2Hook{};
	SafetyHookMid m_gameplayFOV3Hook{};
	SafetyHookMid m_gameplayFOV4Hook{};

	uintptr_t m_gameplayFOV1Address = 0;

	enum destInstruction
	{
		FADD,
		EAX,
		EDX,
		ECX
	};

	enum CameraFOVInstructionsIndices
	{
		MainMenu,
		Cutscenes1,
		Cutscenes2,
		Cutscenes3,
		Gameplay1,
		Gameplay2,
		Gameplay3,
		Gameplay4
	};

	void CameraFOVMidHook(uintptr_t fovAddress, float fovFactor, destInstruction DestInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(fovAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;

		switch (DestInstruction)
		{
			case FADD:
			{
				FPU::FADD(m_newCameraFOV);
				break;
			}

			case EAX:
			{
				ctx.eax = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}

			case EDX:
			{
				ctx.edx = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}

			case ECX:
			{
				ctx.ecx = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}
		}
	}

	inline static PowerdromeFix* s_instance_ = nullptr;
};

static std::unique_ptr<PowerdromeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<PowerdromeFix>(hModule);
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