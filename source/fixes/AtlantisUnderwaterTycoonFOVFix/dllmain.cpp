#include "..\..\common\FixBase.hpp"

class AtlantisUnderwaterTycoonFix final : public FixBase
{
public:
	explicit AtlantisUnderwaterTycoonFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AtlantisUnderwaterTycoonFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AtlantisUnderwaterTycoonFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Atlantis Underwater Tycoon";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ut.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 83 ?? ?? ?? ?? A3 ?? ?? ?? ?? 8B 8B");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.ebx + 0x2AA04);
				int& iCurrentHeight = Memory::ReadMem(ctx.ebx + 0x2AA08);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->WriteStaticARs();
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 4E", "68 ?? ?? ?? ?? 8D 54 24 ?? 68 ?? ?? ?? ?? 52 E8",
		"8B 54 24 ?? 51 55");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], [](SafetyHookContext& ctx)
			{
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B 4E", "68 ?? ?? ?? ?? 52 E8 ?? ?? ?? ?? 8B 46",
		"68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? A1", "7c ?? 89 5e ?? 5f", "75 ?? 89 5e ?? 5f 5e 5d 5b 83 c4 ?? c2 ?? ?? 89 7e",
		"89 5e ?? 5f 5e 5d 5b 83 c4 ?? c2 ?? ?? 90", "0f 85 ?? ?? ?? ?? d9 44 24 ?? d8 1d ?? ?? ?? ?? df e0 f6 c4 ?? 0f 84 ?? ?? ?? ?? 8d be",
		"0f 85 ?? ?? ?? ?? 8b d5");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Culling Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Culling1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Culling Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Culling2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Culling Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Culling3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Culling Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Culling4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera Culling Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Culling5] - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult, FOV1, FOV3, 1, m_newCameraFOV);

			Memory::PatchBytes(CameraFOVScansResult, Culling1, Culling2, 0, "\xEB");
			Memory::PatchBytes(CameraFOVScansResult[Culling3] + 1, "\x7E");
			Memory::PatchBytes(CameraFOVScansResult[Culling4], "\xE9\x3C\x01\x00\x00\x90");
			Memory::PatchBytes(CameraFOVScansResult[Culling5], "\xE9\xA0\x00\x00\x00\x90");
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.75f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_cameraCullingHook{};

	std::vector<std::uint8_t*> AspectRatioScansResult;

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2,
		FOV3,
		Culling1,
		Culling2,
		Culling3,
		Culling4,
		Culling5,
		Culling6
	};

	void WriteStaticARs()
	{
		Memory::Write(AspectRatioScansResult, AR1, AR2, 1, m_newAspectRatio);
	}

	inline static AtlantisUnderwaterTycoonFix* s_instance_ = nullptr;
};

static std::unique_ptr<AtlantisUnderwaterTycoonFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AtlantisUnderwaterTycoonFix>(hModule);
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