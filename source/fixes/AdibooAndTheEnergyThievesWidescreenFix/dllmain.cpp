#include "..\..\common\FixBase.hpp"

class AdibooFix final : public FixBase
{
public:
	explicit AdibooFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AdibooFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AdibooAndTheEnergyThievesWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Adiboo and the Energy Thieves";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Adiboo_Energy_Thieves.exe") || 
		Util::stringcmp_caseless(exeName, "Adiboo_Energy_Config.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "Adiboo_Energy_Config.exe"))
		{
			auto ResolutionListUnlockScansResult = Memory::PatternScan(ExeModule(), "74 ?? 8B 45 ?? 8D 8E", "0F 82 ?? ?? ?? ?? 81 7D");
			if (Memory::AreAllSignaturesValid(ResolutionListUnlockScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScansResult[ResListUnlock1] - (std::uint8_t*)ExeModule());

				spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScansResult[ResListUnlock2] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScansResult[ResListUnlock1], 2);

				Memory::WriteNOPs(ResolutionListUnlockScansResult[ResListUnlock2], 6);

				Memory::WriteNOPs(ResolutionListUnlockScansResult[ResListUnlock2] + 13, 6);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Adiboo_Energy_Thieves.exe"))
		{
			auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 02 A3 ?? ?? ?? ?? 8B 42");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.edx);
					int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);

					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				});
			}
			else
			{
				spdlog::info("Failed to locate the resolution scan memory address.");
				return;
			}

			std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(ExeModule(), "8B 54 24 ?? 81 E6");
			if (CameraFOVInstructionScanResult)
			{
				spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVInstructionScanResult, 4);

				m_cameraFOVHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVMidHook(ctx);
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	enum ResolutionListsIndices
	{
		ResListUnlock1,
		ResListUnlock2
	};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.esp + 0x30);
		m_newCameraFOV = fCurrentCameraFOV * m_aspectRatioScale * m_fovFactor;
		ctx.edx = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static AdibooFix* s_instance_ = nullptr;
};

static std::unique_ptr<AdibooFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AdibooFix>(hModule);
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