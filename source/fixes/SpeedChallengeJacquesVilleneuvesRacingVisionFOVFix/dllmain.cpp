#include "..\..\common\FixBase.hpp"

class SpeedChallengeFix final : public FixBase
{
public:
	explicit SpeedChallengeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SpeedChallengeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SpeedChallengeJacquesVilleneuvesRacingVisionFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Speed Challenge: Jacques Villeneuve's Racing Vision";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "main.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_kernelDllModule = Memory::GetHandle("Kernel.dll");
		m_kernelDllModuleName = Memory::GetModuleName(m_kernelDllModule);

		auto ResolutionScanResult = Memory::PatternScan(m_kernelDllModule, "8b 44 24 ?? 8b 4c 24 ?? 56");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_kernelDllModuleName.c_str(), ResolutionScanResult - (std::uint8_t*)m_kernelDllModule);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x4);
				s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteStaticFOVs();
				spdlog::info("Current res: {}x{}", s_instance_->m_newResX, s_instance_->m_newResY);
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 68 ?? ?? ?? ?? 33 FF",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 52 68 ?? ?? ?? ?? 33 DB", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 55 55 55 55 8B CE");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Starting Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[StartingCam] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GameplayCam] - (std::uint8_t*)ExeModule());
			spdlog::info("Garage Car Camera FOV: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[GarageCam] - (std::uint8_t*)ExeModule());
		}
	}

private:
	HMODULE m_kernelDllModule = nullptr;
	std::string m_kernelDllModuleName = "";

	SafetyHookMid m_resolutionHook{};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newStartingCameraFOV = 0.0f;
	float m_newGameplayCameraFOV = 0.0f;
	float m_newGarageCameraFOV = 0.0f;

	std::vector<std::uint8_t*> CameraFOVScansResult;

	enum CameraFOVInstructionsIndex
	{
		StartingCam,
		GameplayCam,
		GarageCam
	};

	void WriteStaticFOVs()
	{		
		m_newStartingCameraFOV = 0.64999998836f * m_aspectRatioScale;
		m_newGameplayCameraFOV = 0.64999998836f * m_aspectRatioScale * m_fovFactor;
		m_newGarageCameraFOV = 0.5f * m_aspectRatioScale;

		Memory::Write(CameraFOVScansResult[StartingCam] + 1, m_newStartingCameraFOV);
		Memory::Write(CameraFOVScansResult[GameplayCam] + 1, m_newGameplayCameraFOV);
		Memory::Write(CameraFOVScansResult[GarageCam] + 1, m_newGarageCameraFOV);		
	}

	inline static SpeedChallengeFix* s_instance_ = nullptr;
};

static std::unique_ptr<SpeedChallengeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<SpeedChallengeFix>(hModule);
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