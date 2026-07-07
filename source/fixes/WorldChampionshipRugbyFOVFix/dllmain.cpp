#include "..\..\common\FixBase.hpp"

class WorldChampionshipRugbyFix final : public FixBase
{
public:
	explicit WorldChampionshipRugbyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WorldChampionshipRugbyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "WorldChampionshipRugbyFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "World Championship Rugby";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "WCR.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "89 86 ?? ?? ?? ?? 89 9E ?? ?? ?? ?? 88 96");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Globals Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.eax);
				s_instance_->m_newResY = Memory::ReadRegister(ctx.ebx);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
				s_instance_->m_resolutionHook.disable();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution globals instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? 8B 4C 24 ?? 8B 54 24 ?? D9 1D",
		"D9 44 24 ?? 8B 54 24 ?? D8 0D ?? ?? ?? ?? 8B 44 24 ?? D9 1D", "8B 4C 24 ?? A1 ?? ?? ?? ?? 89 0D", "8B 4C 24 ?? 8B DF",
		"8B 44 24 ?? A3 ?? ?? ?? ?? 5F", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 8B 57", "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 54 24",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 5E C2");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV7] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV8] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult, FOV1, FOV5, 0, 4);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x0, DEG, s_instance_->m_fovFactor, FLD, ctx);
			});

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x28, DEG, 1.0f, FLD, ctx);
			});

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x10, RAD, 1.0f, ECX, ctx);
			});

			m_cameraFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x10, RAD, 1.0f, ECX, ctx);
			});

			m_cameraFOV5Hook = safetyhook::create_mid(CameraFOVScansResult[FOV5], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.esp + 0x8, RAD, 1.0f, EAX, ctx);
			});

			m_cameraFOV6Address = Memory::GetPointerFromAddress(CameraFOVScansResult[FOV6] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult, FOV6, FOV8, 0, 10);
			
			m_cameraFOV6Hook = safetyhook::create_mid(CameraFOVScansResult[FOV6], [](SafetyHookContext& ctx)
			{
				s_instance_->StaticCameraFOVsMidHook(m_originalCameraFOV6);
			});

			m_cameraFOV7Hook = safetyhook::create_mid(CameraFOVScansResult[FOV7], [](SafetyHookContext& ctx)
			{
				s_instance_->StaticCameraFOVsMidHook(m_originalCameraFOV7);
			});

			m_cameraFOV8Hook = safetyhook::create_mid(CameraFOVScansResult[FOV8], [](SafetyHookContext& ctx)
			{
				s_instance_->StaticCameraFOVsMidHook(m_originalCameraFOV6);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV6 = 1.04719758f;
	static constexpr float m_originalCameraFOV7 = 0.07196900249f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};
	SafetyHookMid m_cameraFOV5Hook{};
	SafetyHookMid m_cameraFOV6Hook{};
	SafetyHookMid m_cameraFOV7Hook{};
	SafetyHookMid m_cameraFOV8Hook{};

	uintptr_t m_cameraFOV6Address = 0;

	float m_newCameraFOV6 = 0.0f;
	float m_newCameraFOV7 = 0.0f;
	float m_newCameraFOV8 = 0.0f;

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7,
		FOV8
	};

	enum DestInstruction
	{
		FLD,
		EAX,
		ECX
	};

	enum AngleMode
	{
		DEG,
		RAD
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, AngleMode angleMode, float fovFactor, DestInstruction destInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		switch (angleMode)
		{
			case DEG:
			{
				m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;
				break;
			}

			case RAD:
			{
				m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;
				break;
			}
		}

		switch (destInstruction)
		{
			case FLD:
			{
				FPU::FLD(m_newCameraFOV);
				break;
			}

			case EAX:
			{
				ctx.eax = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}

			case ECX:
			{
				ctx.ecx = std::bit_cast<uintptr_t>(m_newCameraFOV);
				break;
			}
		}
	}

	void StaticCameraFOVsMidHook(float sourceFOV)
	{
		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(sourceFOV, m_aspectRatioScale);
		*reinterpret_cast<float*>(m_cameraFOV6Address) = m_newCameraFOV;
	}

	inline static WorldChampionshipRugbyFix* s_instance_ = nullptr;
};

static std::unique_ptr<WorldChampionshipRugbyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<WorldChampionshipRugbyFix>(hModule);
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