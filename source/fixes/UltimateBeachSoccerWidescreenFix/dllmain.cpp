#include "..\..\common\FixBase.hpp"

class UltimateBeachSoccerFix final : public FixBase
{
public:
	explicit UltimateBeachSoccerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~UltimateBeachSoccerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "UltimateBeachSoccerWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Ultimate Beach Soccer";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "BeachSoccer.exe") || 
		Util::stringcmp_caseless(exeName, "Resolution.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "Resolution.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "85 C0 74 ?? 83 7D ?? ?? 75 ?? 81 7D");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 82);
			}
			else
			{
				spdlog::info("Failed to locate resolution list unlock scan memory address.");
				return;
			}
		}		

		if (Util::stringcmp_caseless(ExeName(), "BeachSoccer.exe"))
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
					s_instance_->WriteAR();
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}

			AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 50",
			"68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 5E", "68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 8B CE");
			if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
			{
				spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
				spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
				spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			}

			auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 86 ?? ?? ?? ?? 83 C4 ?? 8B CE 68");
			if (CameraFOVScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVScanResult, 6);

				m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV = Memory::ReadMem(ctx.esi + 0xCC);
					s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction memory address.");
				return;
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	std::vector<std::uint8_t*> AspectRatioScansResult;

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2,
		AR3
	};

	void WriteAR()
	{
		Memory::Write(AspectRatioScansResult, AR1, AR3, 1, m_newAspectRatio);
	}

	inline static UltimateBeachSoccerFix* s_instance_ = nullptr;
};

static std::unique_ptr<UltimateBeachSoccerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<UltimateBeachSoccerFix>(hModule);
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