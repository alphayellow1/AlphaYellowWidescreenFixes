#include "..\..\common\FixBase.hpp"

class BarbieExplorerFix final : public FixBase
{
public:
	explicit BarbieExplorerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BarbieExplorerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BarbieExplorerWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Barbie Explorer";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Barbiex.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;		

		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 83 C4");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(ResolutionScanResult + 6,  m_newResX);
			Memory::Write(ResolutionScanResult + 16, m_newResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "DB 45 D8 D8 0D 60 BD 5D 00 D8 0D 68 BD 5D 00", "DB 45 D4 D8 0D 64 BD 5D 00 D8 0D 68 BD 5D 00 DE C1",
		"D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D 94 BD 5D 00 D9 05 64 BD 5D 00 D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D 98 BD 5D 00 D9 05 68 BD 5D 00 D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D A0 BD 5D 00 DB 05 58 BE 5D 00 D8 05 94 BD 5D 00 D9 1D 60 BD 5D 00 DB 05 64 BE 5D 00 D8 05 98 BD 5D 00 D9 1D 64 BD 5D 00 DB 05 54 BE 5D 00 D8 05 A0 BD 5D 00 D9 15 68 BD 5D 00 D8 1D 00 F3 48 00 DF E0 F6 C4 01 74 16 C7 05 68 BD 5D 00 00 00 00 00 A1 28 BD 5D 00 0C 24 A3 28 BD 5D 00 D9 05 68 BD 5D 00 D8 1D 24 F3 48 00 DF E0 F6 C4 41 75 19 C7 05 68 BD 5D 00 00 FF 7F 47 8B 0D 28 BD 5D 00 83 C9 24 89 0D 28 BD 5D 00 0F BF 05 F2 BD 5D 00 99 2B C2 D1 F8 89 45 BC DB 45 BC D8 1D 68 BD 5D 00 DF E0 F6 C4 01 74 6E 8B 15 68 BD 5D 00 89 15 1C BE 5D 00 D9 05 10 F3 48 00 D8 35",
		"D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D 98 BD 5D 00 D9 05 68 BD 5D 00 D8 0D 10 F3 48 00 D8 35 18 F3 48 00 D9 1D A0 BD 5D 00 DB 05 58 BE 5D 00 D8 05 94 BD 5D 00 D9 1D 60 BD 5D 00 DB 05 64 BE 5D 00 D8 05 98 BD 5D 00 D9 1D 64 BD 5D 00 DB 05 54 BE 5D 00 D8 05 A0 BD 5D 00 D9 15 68 BD 5D 00 D8 1D 00 F3 48 00 DF E0 F6 C4 01 74 16 C7 05 68 BD 5D 00 00 00 00 00 A1 28 BD 5D 00 0C 24 A3 28 BD 5D 00 D9 05 68 BD 5D 00 D8 1D 24 F3 48 00 DF E0 F6 C4 41 75 19 C7 05 68 BD 5D 00 00 FF 7F 47 8B 0D 28 BD 5D 00 83 C9 24 89 0D 28 BD 5D 00 0F BF 05 F2 BD 5D 00 99 2B C2 D1 F8 89 45 BC DB 45 BC D8 1D 68 BD 5D 00 DF E0 F6 C4 01 74 6E 8B 15 68 BD 5D 00");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera Foreground HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ForegroundHFOV] + 3 - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Foreground VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ForegroundVFOV] + 3 - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Background HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[BackgroundHFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Background VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[BackgroundVFOV] - (std::uint8_t*)ExeModule());

			// First function: X side flags
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CA46, 9);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CA93, 9);

			// First function: Y flags
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CA12, 9);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CA2C, 9);

			// First function: previous transform/clamp warning flag
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CAA5, 9);

			// First function: far-depth flag
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CA82, 9);

			// Second function: X side flags
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CC10, 9);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CC5D, 8);

			// Second function: Y flags
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CBDE, 9);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CBF7, 9);

			// Second function: previous transform/clamp warning flag
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CC6E, 9);

			// Second function: far-depth flag
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x4CC4C, 9);

			m_newCameraHFOV = (1.0f / m_fovFactor) / m_aspectRatioScale;
			m_newCameraVFOV = 1.0f / m_fovFactor;

			m_cameraForegroundHFOVHook = safetyhook::create_mid(CameraFOVScansResult[ForegroundHFOV] + 3, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_newCameraHFOV);
			});

			m_cameraForegroundVFOVHook = safetyhook::create_mid(CameraFOVScansResult[ForegroundVFOV] + 3, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_newCameraVFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[BackgroundHFOV], 6);

			m_cameraBackgroundHFOVHook = safetyhook::create_mid(CameraFOVScansResult[BackgroundHFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_newCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[BackgroundVFOV], 6);

			m_cameraBackgroundVFOVHook = safetyhook::create_mid(CameraFOVScansResult[BackgroundVFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_newCameraVFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	SafetyHookMid m_cameraForegroundHFOVHook{};
	SafetyHookMid m_cameraForegroundVFOVHook{};
	SafetyHookMid m_cameraBackgroundHFOVHook{};
	SafetyHookMid m_cameraBackgroundVFOVHook{};

	void CameraFOVMidHook(float newCameraFOV)
	{
		FPU::FMUL(newCameraFOV);
	}

	enum CameraFOVInstructionsIndex
	{
		ForegroundHFOV,
		ForegroundVFOV,
		BackgroundHFOV,
		BackgroundVFOV
	};

	inline static BarbieExplorerFix* s_instance_ = nullptr;
};

static std::unique_ptr<BarbieExplorerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BarbieExplorerFix>(hModule);
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