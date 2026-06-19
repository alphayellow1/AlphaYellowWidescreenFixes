#include "..\..\common\FixBase.hpp"

class CasinoIncFix final : public FixBase
{
public:
	explicit CasinoIncFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~CasinoIncFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "CasinoIncWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Casino Inc.";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Casino.exe") ||
		Util::stringcmp_caseless(exeName, "CasinoExpansionPack.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "MainMenuWidth", m_mewMenuResX);
		inipp::get_value(ini.sections["Settings"], "MainMenuHeight", m_mewMenuResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_mewMenuResX, m_mewMenuResY);

		spdlog_confparse(m_mewMenuResX);
		spdlog_confparse(m_mewMenuResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "0F 86 ?? ?? ?? ?? 81 3D ?? ?? ?? ?? C0 03 00 00", "0F 82 ?? ?? ?? ?? 81 F9 40 06",
		"FF 24 8D ?? ?? ?? ?? B8 18 00 00 00", "74 ?? 8B 41 ?? 2B C2 8B 54 24 ?? C1 F8 ?? 3B D0 73 ?? 8B 49 ??", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B F1 E8 ?? ?? ?? ?? A0 ?? ?? ?? ??",
		"8B 74 24 ?? 57 8B 7C 24 ?? 3B F0");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock3] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 4 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock4] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[MainMenu] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(ResolutionScansResult[ListUnlock1], "\xE9\x99\x00\x00\x00\x90");
			Memory::PatchBytes(ResolutionScansResult[ListUnlock1] + 16, "\xE9\x89\x00\x00\x00\x90");
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2], 6);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2] + 12, 6);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2] + 27, 6);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2] + 38, 6);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2] + 71, 6);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2] + 82, 6);
			Memory::PatchBytes(ResolutionScansResult[ListUnlock3], "\xEB\x15");
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock3] + 2, 5);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock4], 2);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock4] + 16, 2);

			Memory::Write(ResolutionScansResult[MainMenu] + 6, m_mewMenuResX);
			Memory::Write(ResolutionScansResult[MainMenu] + 1, m_mewMenuResY);

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadMem(ctx.esp + 0x18);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight] + 5, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHeight = Memory::ReadMem(ctx.esp + 0x20);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
			});			
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "8B 81 ?? ?? ?? ?? 52 8B 91");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 6);

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

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 91 ?? ?? ?? ?? 50 81 C1");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentCameraFOV = Memory::ReadMem(ctx.ecx + 0x200);
				s_instance_->m_newCameraFOV = s_instance_->m_currentCameraFOV * s_instance_->m_fovFactor;
				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 45.0f;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	int m_currentWidth = 0;
	int m_currentHeight = 0;
	int m_mewMenuResX = 0;
	int m_mewMenuResY = 0;

	float m_currentCameraFOV = 0.0f;

	enum ResolutionInstructionsIndices
	{
		ListUnlock1,
		ListUnlock2,
		ListUnlock3,
		ListUnlock4,
		MainMenu,
		WidthHeight
	};

	inline static CasinoIncFix* s_instance_ = nullptr;
};

static std::unique_ptr<CasinoIncFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<CasinoIncFix>(hModule);
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