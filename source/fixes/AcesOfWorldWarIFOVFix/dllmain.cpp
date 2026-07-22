#include "..\..\common\FixBase.hpp"

class AcesWW1Fix final : public FixBase
{
public:
	explicit AcesWW1Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AcesWW1Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AcesOfWorldWarIFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3.1";
	}

	const char* TargetName() const override
	{
		return "Aces of World War I";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "aces.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "56 E8 ?? ?? ?? ?? 85 C0 0F 8D");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				uintptr_t settings = ctx.ecx;

				uint32_t deviceIndex = *(uint32_t*)(settings + 0x2A380);

				uintptr_t device = settings + 0x4 + deviceIndex * 0x438C;

				uint32_t modeGroup = *(uint32_t*)(device + 0x4388);

				uint32_t resIndex = *(uint32_t*)(device + modeGroup * 0xCA8 + 0x10DC);

				uintptr_t resEntry = device + modeGroup * 0xCA8 + 0x0524 + resIndex * 0x14;

				s_instance_->m_newResX = *(int*)(resEntry);
				s_instance_->m_newResY = *(int*)(resEntry + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 47 ?? ?? ?? ?? ?? C7 47 ?? ?? ?? ?? ?? 8B 8E");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());			

			Memory::WriteNOPs(AspectRatioScanResult, 7);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio2 = m_originalAspectRatio / s_instance_->m_aspectRatioScale;

				*reinterpret_cast<float*>(ctx.edi + 0x2C) = s_instance_->m_newAspectRatio2;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 5F");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 12);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraFOV = Maths::DegToRad(m_originalCameraFOV) * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "E8 ?? ?? ?? ?? 83 BE ?? ?? ?? ?? ?? 0F 84 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 83 BE");
			if (SkipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Scan Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideosScanResult, "\xE9\x23\x00\x00\x00");
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos scan memory address.");
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalAspectRatio = 0.75f;
	static constexpr float m_originalCameraFOV = 42.5f;

	bool m_skipIntroVideos = false;

	uintptr_t m_cameraFOVAddress = 0;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	inline static AcesWW1Fix* s_instance_ = nullptr;
};

static std::unique_ptr<AcesWW1Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AcesWW1Fix>(hModule);
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