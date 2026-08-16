#include "..\..\common\FixBase.hpp"

class DisneysDinosaurFix final : public FixBase
{
public:
	explicit DisneysDinosaurFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DisneysDinosaurFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DisneysDinosaurWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Disney's Dinosaur";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Dx7_Detect.exe") ||
		Util::stringcmp_caseless(exeName, "Dinosaur.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "Dx7_Detect.exe"))
		{
			m_resolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "0F 87 ?? ?? ?? ?? 8D 04 B6");
			if (m_resolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_resolutionListUnlockScanResult, 6);
				Memory::WriteNOPs(m_resolutionListUnlockScanResult + 26, 6);
				Memory::WriteNOPs(m_resolutionListUnlockScanResult + 38, 6);
				Memory::WriteNOPs(m_resolutionListUnlockScanResult + 50, 2);
			}
			else
			{
				spdlog::error("Failed to locate resolution list unlock scan memory address.");
				return;
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Dinosaur.exe"))
		{
			m_resolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 8B 4C 24 ?? A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? C3 90 90 90 90 90 90 90 90 90 90 90 90 A1");
			if (m_resolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScanResult - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(m_resolutionScanResult, [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);
					int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->m_resolutionHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}

			m_aspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? 8B 4C 24 ?? D9 58 ?? D9 44 24");
			if (m_aspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_aspectRatioScanResult, 4);

				m_aspectRatioHook = safetyhook::create_mid(m_aspectRatioScanResult, [](SafetyHookContext& ctx)
				{
					float& fCurrentHFOV = Memory::ReadMem(ctx.esp + 0x8);
					s_instance_->m_newAspectRatio2 = Maths::CalculateNewHFOV_RadBased(fCurrentHFOV, s_instance_->m_aspectRatioScale);
					FPU::FLD(s_instance_->m_newAspectRatio2);
				});
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction memory address.");
				return;
			}

			m_cameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 54 24 ?? 8B 44 24 ?? 8B 4C 24 ?? 52 8B 54 24 ?? 50 51 52 55");
			if (m_cameraFOVScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_cameraFOVScanResult, 4);

				m_cameraFOVHook = safetyhook::create_mid(m_cameraFOVScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentCameraFOV = Memory::ReadMem(ctx.esp + 0x24);
					s_instance_->m_newCameraFOV = s_instance_->m_currentCameraFOV * s_instance_->m_fovFactor;
					ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
				});
			}
			else
			{
				spdlog::info("Failed to locate the camera FOV instruction memory address.");
				return;
			}

			if (m_runMultipleInstances == true)
			{
				m_runMultipleInstancesScanResult = Memory::PatternScan(ExeModule(), "75 ?? 5F 5E 5D 83 C8 ?? 5B 81 C4 ?? ?? ?? ?? C2 ?? ?? 8B 1D");
				if (m_runMultipleInstancesScanResult)
				{
					spdlog::info("Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_runMultipleInstancesScanResult - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(m_runMultipleInstancesScanResult, "\xEB");
				}
				else
				{
					spdlog::info("Failed to locate the multiple instances check instruction memory address.");
					return;
				}
			}			
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	HMODULE m_dllModule2 = nullptr;
	std::string m_dllModule2Name = "";

	bool m_runMultipleInstances = false;

	std::uint8_t* m_resolutionListUnlockScanResult = nullptr;
	std::uint8_t* m_resolutionScanResult = nullptr;
	std::uint8_t* m_aspectRatioScanResult = nullptr;
	std::uint8_t* m_cameraFOVScanResult = nullptr;
	std::uint8_t* m_runMultipleInstancesScanResult = nullptr;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	float m_currentCameraFOV = 0.0f;
	float m_newCameraFOV = 0.0f;

	inline static DisneysDinosaurFix* s_instance_ = nullptr;
};

static std::unique_ptr<DisneysDinosaurFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<DisneysDinosaurFix>(hModule);
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