#include "..\..\common\FixBase.hpp"

class PerfectAce1Fix final : public FixBase
{
public:
	explicit PerfectAce1Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PerfectAce1Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "PerfectAceProTournamentTennisWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Perfect Ace: Pro Tournament Tennis";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ACEPC.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "75 ?? 81 7D ?? ?? ?? ?? ?? 75 ?? 8D 85", "7C ?? 81 7D ?? ?? ?? ?? ?? 7D ?? FF B5",
		"74 ?? 81 7D ?? ?? ?? ?? ?? 7C", "8B 13 89 15 ?? ?? ?? ?? 8B 43");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 3: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1], 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock1] + 9, 2);
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock2], 2);
			Memory::PatchBytes(ResolutionScansResult[ResListUnlock2] + 9, "\xEB\x1F");
			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock3], 2);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.ebx);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.ebx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 B4 81 ?? ?? ?? ?? D9 5D ?? 8D 45");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 7);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FDIV(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 84 81 ?? ?? ?? ?? D8 8C 96 ?? ?? ?? ?? D9 5D ?? 8B 45 ?? 8B 4D ?? 8B 55 ?? 8B 75 ?? D9 84 81 ?? ?? ?? ?? D8 8C 96 ?? ?? ?? ?? 8B 45 ?? 8B 4D ?? E9",
		"D9 84 81 ?? ?? ?? ?? D8 65 ?? D9 1D ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? 0F B6 80 ?? ?? ?? ?? 85 C0 74 ?? 8B 85 ?? ?? ?? ?? 80 A0 ?? ?? ?? ?? ?? D9 45 ?? D9 5D ?? D9 45 ?? D9 5D ?? D9 45");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 7);

			m_generalFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx, s_instance_->m_aspectRatioScale, 1.0f);
			});

			Memory::WriteNOPs(CameraFOVScansResult[General] + 29, 7);

			m_generalFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[General] + 29, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx, s_instance_->m_aspectRatioScale, 1.0f);
			});

			m_newGameplayFOV = m_originalCameraFOV * m_fovFactor;

			Memory::PatchBytes(CameraFOVScansResult[Gameplay], "\xD9\x05");
			Memory::WriteNOPs(CameraFOVScansResult[Gameplay] + 6, 1);

			Memory::Write(CameraFOVScansResult[Gameplay] + 2, &m_newGameplayFOV);
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instructions scan memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.4149999917f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_generalFOV1Hook{};
	SafetyHookMid m_generalFOV2Hook{};
	SafetyHookMid m_gameplayFOVHook{};

	float m_newGameplayFOV = 0.0f;

	enum ResolutionInstructionsIndices
	{
		ResListUnlock1,
		ResListUnlock2,
		ResListUnlock3,
		ResWidthHeight
	};

	enum CameraFOVInstructionsIndex
	{
		General,
		Gameplay
	};

	void CameraFOVMidHook(SafetyHookContext& ctx, float arScale, float fovFactor)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.ecx + ctx.eax * 0x4 + 0xAC);
		m_newCameraFOV = fCurrentCameraFOV * arScale * fovFactor;
		FPU::FLD(m_newCameraFOV);
	}

	inline static PerfectAce1Fix* s_instance_ = nullptr;
};

static std::unique_ptr<PerfectAce1Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<PerfectAce1Fix>(hModule);
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