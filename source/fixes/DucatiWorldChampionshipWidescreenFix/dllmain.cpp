#include "..\..\common\FixBase.hpp"

class DucatiWCFix final : public FixBase
{
public:
	explicit DucatiWCFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DucatiWCFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DucatiWorldChampionshipWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Ducati World Championship";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "GP1.eee");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 4C 24 ?? 81 F9 ?? ?? ?? ?? 56", "8B 0A 89 0D ?? ?? ?? ?? 8B 4A");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(ResolutionScansResult[ResListUnlock], "\xB8\x01\x00\x00\x00\xC3"); // mov eax,01; ret -> this means every resolution passes the validation check, EAX is a boolean value in this case

			Memory::WriteNOPs(ResolutionScansResult[ResListUnlock] + 6, 115); // NOPing out the rest of the function, since it's just useless

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;

				s_instance_->WriteAR();
			});
		}

		BikeSelectionAspectRatioScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B 56");
		if (BikeSelectionAspectRatioScanResult)
		{
			spdlog::info("Bike Selection Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), BikeSelectionAspectRatioScanResult - (std::uint8_t*)ExeModule());
		}
		else
		{
			spdlog::error("Failed to locate bike selection aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 8E ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 51",
		"D9 44 24 ?? D8 25 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? D8 25 ?? ?? ?? ?? DE F9 D9 5C 24 ?? D9 05", "8B 86 ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 50");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0x1E0, ECX, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 4);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x18, FLD, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV3], 6);

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esi + 0x1E0, EAX, ctx);
			});
		}
	}

private:
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_newBikeSelectionAspectRatio = 0.0f;

	uint8_t* BikeSelectionAspectRatioScanResult;

	enum ResolutionInstructionsIndices
	{
		ResListUnlock,
		ResWidthHeight
	};	

	void WriteAR()
	{
		m_newBikeSelectionAspectRatio = 1.0f * m_aspectRatioScale;
		Memory::Write(BikeSelectionAspectRatioScanResult + 1, m_newBikeSelectionAspectRatio);
	}

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3
	};

	enum destInstruction
	{
		FLD,
		EAX,
		ECX
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, destInstruction DestInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;

		switch (DestInstruction)
		{
		case FLD:
			FPU::FLD(m_newCameraFOV);
			break;

		case EAX:
			ctx.eax = std::bit_cast<uintptr_t>(m_newCameraFOV);
			break;

		case ECX:
			ctx.ecx = std::bit_cast<uintptr_t>(m_newCameraFOV);
			break;
		}
	}

	inline static DucatiWCFix* s_instance_ = nullptr;
};

static std::unique_ptr<DucatiWCFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<DucatiWCFix>(hModule);
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
