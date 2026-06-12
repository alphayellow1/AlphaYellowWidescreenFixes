#include "..\..\common\FixBase.hpp"

class AsterixAndObelixXXL2Fix final : public FixBase
{
public:
	explicit AsterixAndObelixXXL2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AsterixAndObelixXXL2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Asterix&ObelixXXL2MissionLasVegumWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Asterix & Obelix XXL 2: Mission: Las Vegum";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "GameModule.elb");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "75 ?? 81 7C 24 ?? ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 7F ?? 46",
		"75 ?? 81 7C 24 ?? ?? ?? ?? ?? 75 ?? 81 7C 24 ?? ?? ?? ?? ?? 7F ?? 4E", "8B 0A 89 0D ?? ?? ?? ?? 8B 6A");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock1], 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock1] + 10, 2);
			Memory::PatchBytes(ResolutionScansResult[ListUnlock1] + 20, "\xEB\x0E");
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2], 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2] + 10, 2);
			Memory::PatchBytes(ResolutionScansResult[ListUnlock2] + 20, "\xEB\x0B");

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->m_newAspectRatio2 = 0.75f / s_instance_->m_aspectRatioScale;
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "8B 15 ?? ?? ?? ?? 89 51 ?? EB", "D9 05 ?? ?? ?? ?? 8B 51");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[CameraAR] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HUDAR] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[CameraAR], 6);

			m_cameraAspectRatioHook = safetyhook::create_mid(AspectRatioScansResult[CameraAR], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio2);

				Memory::Write(s_instance_->m_hudAspectRatioAddress, s_instance_->m_newAspectRatio2);
			});

			m_hudAspectRatioAddress = Memory::GetPointerFromAddress(AspectRatioScansResult[HUDAR] + 2, Memory::PointerMode::Absolute);			
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 41 ?? D8 0D");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 4);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
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
	SafetyHookMid m_cameraAspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	uintptr_t m_hudAspectRatioAddress = 0;

	enum ResolutionInstructionsIndex
	{
		ListUnlock1,
		ListUnlock2,
		ResWidthHeight
	};

	enum AspectRatioInstructionsIndex
	{
		CameraAR,
		HUDAR
	};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x14);
		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;
		FPU::FLD(m_newCameraFOV);
	}

	inline static AsterixAndObelixXXL2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<AsterixAndObelixXXL2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AsterixAndObelixXXL2Fix>(hModule);
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