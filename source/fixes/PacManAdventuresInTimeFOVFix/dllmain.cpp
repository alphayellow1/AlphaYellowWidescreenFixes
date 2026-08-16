#include "..\..\common\FixBase.hpp"

class PacManAdventuresInTimeFix final : public FixBase
{
public:
	explicit PacManAdventuresInTimeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~PacManAdventuresInTimeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "PacManAdventuresInTimeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.4.1";
	}

	const char* TargetName() const override
	{
		return "Pac-Man: Adventures in Time";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "pac-man.exe");
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
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 46 ?? A3 ?? ?? ?? ?? 8B 4E ?? 89 0D ?? ?? ?? ?? 8B 56 ?? 89 15 ?? ?? ?? ?? 8B 46 ?? A3 ?? ?? ?? ?? 8B 4E ?? 89 0D ?? ?? ?? ?? 8B 56 ?? 89 15 ?? ?? ?? ?? 8B 46 ?? A3 ?? ?? ?? ?? 8B 4E ?? 89 0D ?? ?? ?? ?? 8A 97",
		"ff 51 ?? 85 c0 7d ?? c6 05", "ff 52 ?? 85 c0 7d ?? c6 05");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());
			spdlog::info("DirectDraw Surface Blit Call Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[DDrawBlit1] - (std::uint8_t*)ExeModule());
			spdlog::info("DirectDraw Surface Blit Call Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[DDrawBlit2] - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esi + 0x14);
				int& iCurrentHeight = Memory::ReadMem(ctx.esi + 0x18);

				s_instance_->m_currentWidth = iCurrentWidth;
				s_instance_->m_currentHeight = iCurrentHeight;

				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;				
			});

			m_bltHook1 = safetyhook::create_mid(ResolutionScansResult[DDrawBlit1], [](SafetyHookContext& ctx)
			{
				s_instance_->SetDestinationRect(ctx, s_instance_->m_videoRect1);
			});

			m_bltHook2 = safetyhook::create_mid(ResolutionScansResult[DDrawBlit2], [](SafetyHookContext& ctx)
			{
				s_instance_->SetDestinationRect(ctx, s_instance_->m_videoRect2);
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 55");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScanResult, 8);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newAspectRatio2 = 1.0f / s_instance_->m_newAspectRatio;
				*reinterpret_cast<float*>(ctx.esp + 0x34) = s_instance_->m_newAspectRatio2;
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 51 68 ?? ?? ?? ?? B9", "68 ?? ?? ?? ?? 51 68 ?? ?? ?? ?? 8D 8E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScansResult[FOV1] + 1, m_newCameraFOV);
			Memory::Write(CameraFOVScansResult[FOV2] + 1, m_newCameraFOV);
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroLogosScanResult = Memory::PatternScan(ExeModule(), "7D ?? C1 E0 ?? 8D 8C 30");
			if (SkipIntroLogosScanResult)
			{
				spdlog::info("Skip Intro Logos Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroLogosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroLogosScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate skip intro logos instruction memory address.");
				return;
			}
		}		

		auto AltTabFixScansResult = Memory::PatternScan(ExeModule(), "6A ?? 51 8B 10 50 FF 52 ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 84 C0 0F 94 C0 C3 90 90 90 90 90 90 90 90 90 90 90 90 90 A1",
		"6A ?? 52 8B 08 50 FF 51 ?? 68");
		if (Memory::AreAllSignaturesValid(AltTabFixScansResult))
		{
			spdlog::info("Alt-Tab Fix: Runtime keyboard cooperative-level flags found at {:s}+{:x}", ExeName().c_str(), AltTabFixScansResult[RuntimeKeyboardCooperativeLevel] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Alt-Tab Fix: Initial keyboard cooperative-level flags found at {:s}+{:x}", ExeName().c_str(), AltTabFixScansResult[InitialKeyboardCooperativeLevel] - reinterpret_cast<std::uint8_t*>(ExeModule()));

			// DISCL_NONEXCLUSIVE
			constexpr std::uint8_t NonExclusiveForeground = 0x06;

			Memory::PatchBytes(AltTabFixScansResult[RuntimeKeyboardCooperativeLevel] + 1, NonExclusiveForeground);
			Memory::PatchBytes(AltTabFixScansResult[InitialKeyboardCooperativeLevel] + 1, NonExclusiveForeground);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 1.0471975803375244f;

	bool m_skipIntroVideos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_bltHook1{};
	SafetyHookMid m_bltHook2{};

	RECT m_videoRect1{};
	RECT m_videoRect2{};

	LONG m_currentWidth = 640;
	LONG m_currentHeight = 480;

	static constexpr std::uintptr_t m_binkHandleRVA = 0x2B08478;

	struct BinkHeader
	{
		std::uint32_t width;
		std::uint32_t height;
	};

	RECT MakeCenteredRect()
	{
		const auto bink = *reinterpret_cast<BinkHeader**>(reinterpret_cast<std::uintptr_t>(ExeModule()) + m_binkHandleRVA);

		if (bink == nullptr || bink->width == 0 || bink->height == 0 || m_currentWidth <= 0 || m_currentHeight <= 0)
		{
			return RECT{0, 0, m_currentWidth, m_currentHeight};
		}

		const std::int64_t videoWidth = bink->width;
		const std::int64_t videoHeight = bink->height;

		LONG scaledWidth{};
		LONG scaledHeight{};

		if (videoWidth * m_currentHeight > static_cast<std::int64_t>(m_currentWidth) * videoHeight)
		{
			scaledWidth = m_currentWidth;
			scaledHeight = static_cast<LONG>(static_cast<std::int64_t>(m_currentWidth) * videoHeight / videoWidth);
		}
		else
		{
			scaledHeight = m_currentHeight;
			scaledWidth = static_cast<LONG>(static_cast<std::int64_t>(m_currentHeight) * videoWidth / videoHeight);
		}

		const LONG left = (m_currentWidth - scaledWidth) / 2;
		const LONG top = (m_currentHeight - scaledHeight) / 2;

		return RECT{left, top, left + scaledWidth, top + scaledHeight};
	}

	void SetDestinationRect(SafetyHookContext& ctx, RECT& storage)
	{
		storage = MakeCenteredRect();
		auto* const lpDestRect = reinterpret_cast<RECT**>(ctx.esp + 0x4);
		*lpDestRect = &storage;
	}

	enum ResolutionInstructionsIndices
	{
		WidthHeight,
		DDrawBlit1,
		DDrawBlit2
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	enum AltTabFixScans
	{
		RuntimeKeyboardCooperativeLevel,
		InitialKeyboardCooperativeLevel
	};

	float m_currentCameraHFOV = 0.0f;
	float m_currentCameraVFOV = 0.0f;
	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	inline static PacManAdventuresInTimeFix* s_instance_ = nullptr;
};

static std::unique_ptr<PacManAdventuresInTimeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<PacManAdventuresInTimeFix>(hModule);
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