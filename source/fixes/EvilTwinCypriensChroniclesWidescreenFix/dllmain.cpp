#include "..\..\common\FixBase.hpp"

class EvilTwinFix final : public FixBase
{
public:
	explicit EvilTwinFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~EvilTwinFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EvilTwinCypriensChroniclesWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Evil Twin: Cyprien's Chronicles";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "video.exe") ||
		Util::stringcmp_caseless(exeName, "EvilTwin.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "video.exe"))
		{
			auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "7F ?? 3B 44 24", "7F ?? 46 83 C1", "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? EB");
			if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());
				spdlog::info("Default Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[DefaultRes] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionScansResult[ListUnlock1], 2);
				Memory::WriteNOPs(ResolutionScansResult[ListUnlock2], 2);

				m_newDefaultWidth = Screen::GetDesktopResolutionWidth();
				m_newDefaultHeight = Screen::GetDesktopResolutionHeight();

				Memory::Write(ResolutionScansResult[DefaultRes] + 4, m_newDefaultWidth);
				Memory::Write(ResolutionScansResult[DefaultRes] + 12, m_newDefaultHeight);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "EvilTwin.exe"))
		{
			m_fnxCoreModule = Memory::GetHandle("fnx_core.dll");
			m_fnxCoreModuleName = Memory::GetModuleName(m_fnxCoreModule);
			m_fnxDX7Module = Memory::GetHandle("fnx_dx7.dll");
			m_fnxDX7ModuleName = Memory::GetModuleName(m_fnxDX7Module);

			auto ResolutionScanResult = Memory::PatternScan(m_fnxDX7Module, "FF 51 ?? 85 C0 0F 85 ?? ?? ?? ?? 6A");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), ResolutionScanResult - (std::uint8_t*)m_fnxDX7Module);

				m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);
					int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
					s_instance_->m_resolutionHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}			

			auto AspectRatioScanResult = Memory::PatternScan(m_fnxCoreModule, "A1 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B C6");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", m_fnxCoreModuleName.c_str(), AspectRatioScanResult - (std::uint8_t*)m_fnxCoreModule);

				Memory::WriteNOPs(AspectRatioScanResult, 5);

				m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
				{
					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio);
				});
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction memory address.");
				return;
			}

			auto CameraFOVScansResult = Memory::PatternScan(m_fnxDX7Module, "8B 8E ?? ?? ?? ?? 52", "D9 87 ?? ?? ?? ?? 8D 4C 24");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)m_fnxDX7Module);
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)m_fnxDX7Module);

				Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

				m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.esi + 0xCC);

					if (fCurrentCameraFOV1 != 1.134464025497f)
					{
						s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
					}

					ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV1);
				});

				Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

				m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.edi + 0xCC);

					if (fCurrentCameraFOV2 != 1.134464025497f)
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale);
					}

					FPU::FLD(s_instance_->m_newCameraFOV2);
				});
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	HMODULE m_fnxDX7Module = nullptr;
	std::string m_fnxDX7ModuleName = "";
	HMODULE m_fnxCoreModule = nullptr;
	std::string m_fnxCoreModuleName = "";

	int m_newDefaultWidth = 0;
	int m_newDefaultHeight = 0;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum ResolutionInstructionsIndex
	{
		ListUnlock1,
		ListUnlock2,
		DefaultRes
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2
	};

	inline static EvilTwinFix* s_instance_ = nullptr;
};

static std::unique_ptr<EvilTwinFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<EvilTwinFix>(hModule);
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