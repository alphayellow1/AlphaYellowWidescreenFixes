#include "..\..\common\FixBase.hpp"

class CivilWarBullRunFix final : public FixBase
{
public:
	explicit CivilWarBullRunFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~CivilWarBullRunFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheHistoryChannelCivilWarTheBattleOfBullRunTakeCommand1861WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "The History Channel: Civil War: The Battle of Bull Run Take Command 1861";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "CWBR.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "81 F9 ?? ?? ?? ?? 75 ?? 3D ?? ?? ?? ?? 74 ?? 81 F9");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Limit Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScanResult, 50);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadMem(ctx.esi + 0x298);
				s_instance_->m_currentHeight = Memory::ReadMem(ctx.esi + 0x29C);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution limit unlock scan memory address.");
			return;
		}

		m_dllModule2 = Memory::GetHandle("powerrender5.dll");
		m_dllModule2Name = Memory::GetModuleName(m_dllModule2);

		auto AspectRatioScanResult = Memory::PatternScan(m_dllModule2, "C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 5E");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), AspectRatioScanResult - (std::uint8_t*)m_dllModule2);

			Memory::WriteNOPs(AspectRatioScanResult, 7);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esi + 0x50) = s_instance_->m_newAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(m_dllModule2, "C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 5E C2");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), CameraFOVScanResult - (std::uint8_t*)m_dllModule2);

			Memory::WriteNOPs(CameraFOVScanResult, 7);

			m_cameraFOVHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraFOV = Maths::CalculateNewFOV_RadBased(m_originalCameraFOV, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;

				*reinterpret_cast<float*>(ctx.esi + 0x54) = s_instance_->m_newCameraFOV;
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	HMODULE m_dllModule2 = nullptr;
	std::string m_dllModule2Name = "";

	static constexpr float m_oldAspectRatio = 1.6f;
	static constexpr float m_originalCameraFOV = 1.047166705f;

	uint32_t m_currentWidth = 0;
	uint32_t m_currentHeight = 0;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	enum ResolutionInstructionsIndex
	{
		ResLimitUnlock,
		ResWidth,
		ResHeight
	};

	inline static CivilWarBullRunFix* s_instance_ = nullptr;
};

static std::unique_ptr<CivilWarBullRunFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<CivilWarBullRunFix>(hModule);
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