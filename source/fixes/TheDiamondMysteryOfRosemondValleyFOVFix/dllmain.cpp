#include "..\..\common\FixBase.hpp"

class DiamondMysteryFix final : public FixBase
{
public:
	explicit DiamondMysteryFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DiamondMysteryFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheDiamondMysteryOfRosemondValleyFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "The Diamond Mystery of Rosemond Valley";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "RVPGame.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 44 24 ?? 8B 4C 24 ?? 8B 54 24 ?? 89 44 24 ?? 89 4C 24 ?? 89 54 24 ?? A1");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x10);
				s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x24);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::info("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? D9 5C 24 ?? E8 ?? ?? ?? ?? 83 C4", "DA 74 24 ?? C7 44 24");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[HFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[VFOV] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[HFOV], 8);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScansResult[HFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraHFOV = 1.0f * s_instance_->m_aspectRatioScale * s_instance_->m_fovFactor;
				*reinterpret_cast<float*>(ctx.esp + 0x1C) = s_instance_->m_newCameraHFOV;
			});

			Memory::WriteNOPs(CameraFOVScansResult[VFOV], 4);

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScansResult[VFOV], [](SafetyHookContext& ctx)
			{
				int& iCurrentResX2 = *(int*)(ctx.esp + 0x2C);
				s_instance_->m_newResX2 = (int)((iCurrentResX2 / s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor);
				FPU::FIDIV(s_instance_->m_newResX2);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	float m_newCameraHFOV = 0.0f;
	int m_newResX2 = 0;

	enum CameraFOVInstructionsIndices
	{
		HFOV,
		VFOV
	};

	inline static DiamondMysteryFix* s_instance_ = nullptr;
};

static std::unique_ptr<DiamondMysteryFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<DiamondMysteryFix>(hModule);
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