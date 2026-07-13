#include "..\..\common\FixBase.hpp"

class WCS2000Fix final : public FixBase
{
public:
	explicit WCS2000Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WCS2000Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "WorldChampionshipSnookerWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3.1";
	}

	const char* TargetName() const override
	{
		return "World Championship Snooker";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "wcsconf.exe") ||
		Util::stringcmp_caseless(exeName, "wcs.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "wcsconf.exe"))
		{
			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "3B D1 0F 85 ?? ?? ?? ?? 3D");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 30);
			}
			else
			{
				spdlog::error("Failed to locate resolution list unlock scan memory address.");
				return;
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "wcs.exe"))
		{
			auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 42 ?? 89 43 ?? 8B 4A ?? 8B 44 24 ?? 89 4B ?? 8B 52 ?? 89 53 ?? 8B 0D ?? ?? ?? ?? 3B C1 75 ?? 8B 85 ?? ?? ?? ?? 0C ?? 89 85 ?? ?? ?? ?? 5F 5E 5D B8 ?? ?? ?? ?? 5B 83 C4 ?? C3 90 90 90 90 90 90",
			"8B 42 ?? 5F 89 43");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Cue Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[CueView] - (std::uint8_t*)ExeModule());
				spdlog::info("Overhead Table Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[TableOverheadView] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVScansResult[CueView], 3);

				m_cueCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[CueView], [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVMidHook(ctx);
				});

				Memory::WriteNOPs(CameraFOVScansResult[TableOverheadView], 3);

				m_tableOverheadCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[TableOverheadView], [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVMidHook(ctx);
				});
			}

			if (m_runMultipleInstances == true)
			{
				auto MultipleInstancesScanResult = Memory::PatternScan(ExeModule(), "75 ?? 8B 0D ?? ?? ?? ?? 51");
				if (MultipleInstancesScanResult)
				{
					spdlog::info("Multiple Instances Check Scan: Address is {:s}+{:x}", ExeName().c_str(), MultipleInstancesScanResult - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(MultipleInstancesScanResult, "\xEB");
				}
				else
				{
					spdlog::error("Failed to locate multiple instances check memory address.");
					return;
				}
			}

			if (m_skipIntroVideos == true)
			{
				auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "6A ?? 8D 54 24 ?? 33 C9");
				if (SkipIntroVideosScanResult)
				{
					spdlog::info("Skip Intro Videos Check Scan: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScanResult - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(SkipIntroVideosScanResult, "\xE9\x36\x00\x00\x00");
				}
				else
				{
					spdlog::error("Failed to locate skip intro videos check memory address.");
					return;
				}
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_cueCameraFOVHook{};
	SafetyHookMid m_tableOverheadCameraFOVHook{};

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	float m_currentCameraFOV = 0.0f;

	enum CameraFOVScansIndex
	{
		CueView,
		TableOverheadView
	};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		s_instance_->m_currentCameraFOV = Memory::ReadMem(ctx.edx + 0x68);
		s_instance_->m_newCameraFOV = s_instance_->m_currentCameraFOV * s_instance_->m_fovFactor;
		ctx.eax = s_instance_->m_newCameraFOV;
	}

	inline static WCS2000Fix* s_instance_ = nullptr;
};

static std::unique_ptr<WCS2000Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<WCS2000Fix>(hModule);
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