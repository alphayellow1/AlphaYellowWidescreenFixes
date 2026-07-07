#include "..\..\common\FixBase.hpp"

class WorldsGreatestCoasters3DFix final : public FixBase
{
public:
	explicit WorldsGreatestCoasters3DFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WorldsGreatestCoasters3DFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "WorldsGreatestCoasters3DWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "World's Greatest Coasters 3D";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "coaster.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "81 F9 ?? ?? ?? ?? 0F 8C", "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock], 40);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.ecx);
				s_instance_->m_newResY = Memory::ReadRegister(ctx.edx);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->SetARAndFOV();
				s_instance_->m_resolutionHook.disable();
			});
		}

		AspectRatioScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 8B 48");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());			
		}
		else
		{
			spdlog::error("Failed to find aspect ratio instruction memory address.");
			return;
		}

		CameraFOVScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 8B 48");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());			
		}
		else
		{
			spdlog::error("Failed to find camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.6999999881f;

	SafetyHookMid m_resolutionHook{};

	std::uint8_t* AspectRatioScanResult = nullptr;
	std::uint8_t* CameraFOVScanResult = nullptr;

	enum ResolutionInstructionsScansIndices
	{
		ListUnlock,
		WidthHeight
	};

	void SetARAndFOV()
	{
		m_newCameraFOV = m_originalCameraFOV * m_aspectRatioScale * m_fovFactor;

		Memory::Write(AspectRatioScanResult + 1, m_newAspectRatio);
		Memory::Write(CameraFOVScanResult + 1, m_newCameraFOV);
	}

	inline static WorldsGreatestCoasters3DFix* s_instance_ = nullptr;
};

static std::unique_ptr<WorldsGreatestCoasters3DFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<WorldsGreatestCoasters3DFix>(hModule);
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