#include "..\..\common\FixBase.hpp"

class ZanzarahFix final : public FixBase
{
public:
	explicit ZanzarahFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ZanzarahFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ZanZarahTheHiddenPortalWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "ZanZarah: The Hidden Portal";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "zanthp.exe");
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

		auto ResolutionListsScansResult = Memory::PatternScan(ExeModule(), "80 02 00 00 E0 01 00 00 10 00 00 00 20 03 00 00 58 02 00 00 10 00 00 00 00 04 00 00 00 03 00 00 10 00 00 00 80 02 00 00 E0 01 00 00 20 00 00 00 20 03 00 00 58 02 00 00 20 00 00 00 00 04 00 00 00 03 00 00 20 00 00 00 DB",
		"80 02 00 00 E0 01 00 00 10 00 00 00 20 03 00 00 58 02 00 00 10 00 00 00 00 04 00 00 00 03 00 00 10 00 00 00 80 02 00 00 E0 01 00 00 20 00 00 00 20 03 00 00 58 02 00 00 20 00 00 00 00 04 00 00 00 03 00 00 20 00 00 00 F5",
		"80 02 00 00 E0 01 00 00 10 00 00 00 20 03 00 00 58 02 00 00 10 00 00 00 00 04 00 00 00 03 00 00 10 00 00 00 80 02 00 00 E0 01 00 00 20 00 00 00 20 03 00 00 58 02 00 00 20 00 00 00 00 04 00 00 00 03 00 00 20 00 00 00 3B");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[ResList1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[ResList2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[ResList3] - (std::uint8_t*)ExeModule());
			
			// Resolution List 1
			// 640x480x16
			Memory::Write(ResolutionListsScansResult[ResList1], m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 4, m_newResY);
			// 800x600x16
			Memory::Write(ResolutionListsScansResult[ResList1] + 12, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 16, m_newResX);
			// 1024x768x16
			Memory::Write(ResolutionListsScansResult[ResList1] + 24, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 28, m_newResX);
			// 640x480x32
			Memory::Write(ResolutionListsScansResult[ResList1] + 36, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 40, m_newResY);
			// 800x600x32
			Memory::Write(ResolutionListsScansResult[ResList1] + 48, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 52, m_newResX);
			// 1024x768x32
			Memory::Write(ResolutionListsScansResult[ResList1] + 60, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList1] + 64, m_newResX);

			// Resolution List 2
			// 640x480x16
			Memory::Write(ResolutionListsScansResult[ResList2], m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 4, m_newResY);
			// 800x600x16
			Memory::Write(ResolutionListsScansResult[ResList2] + 12, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 16, m_newResX);
			// 1024x768x16
			Memory::Write(ResolutionListsScansResult[ResList2] + 24, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 28, m_newResX);
			// 640x480x32
			Memory::Write(ResolutionListsScansResult[ResList2] + 36, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 40, m_newResY);
			// 800x600x32
			Memory::Write(ResolutionListsScansResult[ResList2] + 48, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 52, m_newResX);
			// 1024x768x32
			Memory::Write(ResolutionListsScansResult[ResList2] + 60, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList2] + 64, m_newResX);

			// Resolution List 3
			// 640x480x16
			Memory::Write(ResolutionListsScansResult[ResList3], m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList3] + 4, m_newResY);
			// 800x600x16
			Memory::Write(ResolutionListsScansResult[ResList3] + 12, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList3] + 16, m_newResX);
			// 1024x768x16
			Memory::Write(ResolutionListsScansResult[ResList3] + 24, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList3] + 28, m_newResX);
			// 640x480x32
			Memory::Write(ResolutionListsScansResult[ResList3] + 36, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList3] + 40, m_newResY);
			// 800x600x32
			Memory::Write(ResolutionListsScansResult[ResList3] + 48, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList3] + 52, m_newResX);
			// 1024x768x32
			Memory::Write(ResolutionListsScansResult[ResList3] + 60, m_newResX);
			Memory::Write(ResolutionListsScansResult[ResList3] + 64, m_newResX);
		}		

		auto HUDScansResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? 51 D9 1C 24 51 8B 4E ?? D9 E8 81 C1 ?? ?? ?? ?? D9 1C 24 E8 ?? ?? ?? ?? C7 86",
		"D9 05 ?? ?? ?? ?? 8B BE", "D9 05 ?? ?? ?? ?? 8B 40 ?? 8B 48 ?? D9 C2",
		"D8 0D ?? ?? ?? ?? DE C1 D9 96 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 45 ?? D9 96 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 5E C9 C2 ?? ?? 55 8B EC 51 51",
		"D8 0D ?? ?? ?? ?? D8 E2 D9 55 ?? D9 9E ?? ?? ?? ?? 8B 45 ?? 89 86 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? DE C1 D9 96 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 45 ?? D9 96 ?? ?? ?? ?? D9 9E ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 5E C9 C2 ?? ?? 55 8B EC 51 51",
		"D8 0D ?? ?? ?? ?? D9 5E ?? D9 C2", "D8 0D ?? ?? ?? ?? D9 5E ?? D9 56 ?? D9 45 ?? D9 56 ?? D9 05 ?? ?? ?? ?? D9 56 ?? D9 EE D9 5E ?? 89 56", "D9 05 ?? ?? ?? ?? D9 05 ?? ?? ?? ?? 83 FF");
		if (Memory::AreAllSignaturesValid(HUDScansResult) == true)
		{
			spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD1] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD2] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD3] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD4] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD5] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD6] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD7] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUD8] - (std::uint8_t*)ExeModule());

			m_HUD1Address = Memory::GetPointerFromAddress(HUDScansResult[HUD1] + 2, Memory::PointerMode::Absolute);
			m_HUD2Address = Memory::GetPointerFromAddress(HUDScansResult[HUD2] + 2, Memory::PointerMode::Absolute);
			m_HUD3Address = Memory::GetPointerFromAddress(HUDScansResult[HUD3] + 2, Memory::PointerMode::Absolute);
			m_HUD4Address = Memory::GetPointerFromAddress(HUDScansResult[HUD4] + 2, Memory::PointerMode::Absolute);
			m_HUD5Address = Memory::GetPointerFromAddress(HUDScansResult[HUD5] + 2, Memory::PointerMode::Absolute);
			m_HUD6Address = Memory::GetPointerFromAddress(HUDScansResult[HUD6] + 2, Memory::PointerMode::Absolute);
			m_HUD7Address = Memory::GetPointerFromAddress(HUDScansResult[HUD7] + 2, Memory::PointerMode::Absolute);
			m_HUD8Address = Memory::GetPointerFromAddress(HUDScansResult[HUD8] + 2, Memory::PointerMode::Absolute);

			m_newHUD1Value = 0.75f / m_aspectRatioScale;
			m_newHUD2Value = 0.65f / m_aspectRatioScale;
			m_newHUD3Value = 0.71f / m_aspectRatioScale;
			m_newHUD4Value = 0.72f / m_aspectRatioScale;
			m_newHUD5Value = 0.68f / m_aspectRatioScale;
			m_newHUD6Value = 0.725f / m_aspectRatioScale;
			m_newHUD7Value = 0.685f / m_aspectRatioScale;
			m_newHUD8Value = 0.69f / m_aspectRatioScale;

			Memory::Write(m_HUD1Address, m_newHUD1Value);
			Memory::Write(m_HUD2Address, m_newHUD2Value);
			Memory::Write(m_HUD3Address, m_newHUD3Value);
			Memory::Write(m_HUD4Address, m_newHUD4Value);
			Memory::Write(m_HUD5Address, m_newHUD5Value);
			Memory::Write(m_HUD6Address, m_newHUD6Value);
			Memory::Write(m_HUD7Address, m_newHUD7Value);
			Memory::Write(m_HUD8Address, m_newHUD8Value);
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 45 ?? 89 45 ?? 8B 45 ?? 56 89 45");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult + 6 - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 3);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx, ctx.ebp + 0x8);
			});

			Memory::WriteNOPs(CameraFOVScanResult + 6, 3);

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScanResult + 6, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx, ctx.ebp + 0xC);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instructions scan memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	uintptr_t m_HUD1Address = 0;
	uintptr_t m_HUD2Address = 0;
	uintptr_t m_HUD3Address = 0;
	uintptr_t m_HUD4Address = 0;
	uintptr_t m_HUD5Address = 0;
	uintptr_t m_HUD6Address = 0;
	uintptr_t m_HUD7Address = 0;
	uintptr_t m_HUD8Address = 0;

	float m_newHUD1Value = 0.0f;
	float m_newHUD2Value = 0.0f;
	float m_newHUD3Value = 0.0f;
	float m_newHUD4Value = 0.0f;
	float m_newHUD5Value = 0.0f;
	float m_newHUD6Value = 0.0f;
	float m_newHUD7Value = 0.0f;
	float m_newHUD8Value = 0.0f;
	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;	

	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	enum ResolutionListsIndex
	{
		ResList1,
		ResList2,
		ResList3
	};

	enum HUDInstructionsIndex
	{
		HUD1,
		HUD2,
		HUD3,
		HUD4,
		HUD5,
		HUD6,
		HUD7,
		HUD8
	};

	void CameraFOVMidHook(SafetyHookContext& ctx, uintptr_t FOVAddress)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);
		m_newCameraFOV = fCurrentCameraFOV * m_aspectRatioScale * m_fovFactor;
		ctx.eax = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static ZanzarahFix* s_instance_ = nullptr;
};

static std::unique_ptr<ZanzarahFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ZanzarahFix>(hModule);
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