#include "..\..\common\FixBase.hpp"

class RGO2002Fix final : public FixBase
{
public:
	explicit RGO2002Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RGO2002Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "RolandGarrosFrenchOpen2002FOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Roland Garros French Open 2002";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "NGT.exe");
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
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 48 ?? 89 0D ?? ?? ?? ?? E8");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.eax + 0xC);
				int& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x10);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_resolutionHook.disable();
			});
		}
		else
		{
			spdlog::info("Failed to locate the resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "89 81 ?? ?? ?? ?? C3 83 EC");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.ecx + 0xC0) = s_instance_->m_newAspectRatio;
			});
		}
		else
		{
			spdlog::info("Failed to locate the aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "89 90 ?? ?? ?? ?? 8D 91");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
			});
		}
		else
		{
			spdlog::info("Failed to locate the camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "0F 85 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(RunMultipleInstancesCheckScanResult, 6);
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "76 ?? BF ?? ?? ?? ?? 89 7E", "0F 8D ?? ?? ?? ?? 6A ?? 6A");
			if (Memory::AreAllSignaturesValid(SkipIntroVideosScansResult) == true)
			{
				spdlog::info("Movie Completion Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[0] - reinterpret_cast<std::uint8_t*>(ExeModule()));
				spdlog::info("Post-Intro Timer Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[1] - reinterpret_cast<std::uint8_t*>(ExeModule()));

				Memory::WriteNOPs(SkipIntroVideosScansResult[MovieCompletionCheck], 2);
				Memory::PatchBytes(SkipIntroVideosScansResult[PostIntroTimerCheck], "\xE9\x03\x01\x00\x00\x90");
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;
	
	enum SkipIntroVideosIntructionsIndex
	{
		MovieCompletionCheck,
		PostIntroTimerCheck
	};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		const float& fCurrentCameraFOV = std::bit_cast<float>(ctx.edx);
		m_newCameraFOV = fCurrentCameraFOV * m_fovFactor;
		*reinterpret_cast<float*>(ctx.eax + 0xEC) = m_newCameraFOV;
	}

	inline static RGO2002Fix* s_instance_ = nullptr;
};

static std::unique_ptr<RGO2002Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<RGO2002Fix>(hModule);
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