#include "..\..\common\FixBase.hpp"

class AFLLivePEFix final : public FixBase
{
public:
	explicit AFLLivePEFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AFLLivePEFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AFLLivePremiershipEditionFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "AFL Live: Premiership Edition";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AFLLIVEPE.exe");
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
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;

				s_instance_->WriteFOV();
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

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_gameplayFOVHook{};
	SafetyHookMid m_cutscenesFOVHook{};
	
	uintptr_t GameplayFOVAddress = 0;
	float m_newLogoFOV = 0.0f;

	enum CameraFOVInstructionsIndices
	{
		Gameplay,
		Cutscenes,
		Logo
	};

	void WriteFOV()
	{
		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? 8B 41 ?? D8 49", "D9 44 24 ?? 8B 4C 24 ?? D8 0D ?? ?? ?? ?? 8B 54 24",
		"68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 44 24 ?? 8B 0E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
			spdlog::info("Cutscenes FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Cutscenes] - (std::uint8_t*)ExeModule());
			spdlog::info("Logo FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Logo] - (std::uint8_t*)ExeModule());

			GameplayFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[Gameplay] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 6);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->GameplayFOVAddress, s_instance_->m_fovFactor, s_instance_->m_aspectRatioScale, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Cutscenes], 4);

			m_cutscenesFOVHook = safetyhook::create_mid(CameraFOVScansResult[Cutscenes], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x30, 1.0f, 1.0f / s_instance_->m_aspectRatioScale, ctx);
			});

			m_newLogoFOV = 1.0f * m_aspectRatioScale;

			Memory::Write(CameraFOVScansResult[Logo] + 1, m_newLogoFOV);
		}
	}

	void CameraFOVMidHook(uintptr_t FOVAddress, float arScale, float fovFactor, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = fCurrentCameraFOV * arScale * fovFactor;

		FPU::FLD(m_newCameraFOV);
	}

	inline static AFLLivePEFix* s_instance_ = nullptr;
};

static std::unique_ptr<AFLLivePEFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AFLLivePEFix>(hModule);
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