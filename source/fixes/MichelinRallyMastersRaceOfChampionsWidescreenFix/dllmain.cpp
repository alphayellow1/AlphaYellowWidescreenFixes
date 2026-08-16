#include "..\..\common\FixBase.hpp"

class RallyMastersFix final : public FixBase
{
public:
	explicit RallyMastersFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RallyMastersFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MichelinRallyMastersRaceOfChampionsWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Michelin Rally Masters: Race of Champions";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "RallyMasters.exe");
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
		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "89 3D ?? ?? ?? ?? B9", "FF 51 ?? 3B C3 74 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53 8B C8 E8 ?? ?? ?? ?? BA ?? ?? ?? ?? B9 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? E8",
		"A1 ?? ?? ?? ?? 53 53 8B 48");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());
			spdlog::info("Bink Video Rect Hook: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkVideoRect] - (std::uint8_t*)ExeModule());
			spdlog::info("Bink Video Handle Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkVideoHandle] - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.ebx);
				s_instance_->m_newResY = Memory::ReadRegister(ctx.edi);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});

			m_binkVideoRectHook = safetyhook::create_mid(m_resolutionScansResult[BinkVideoRect], [](SafetyHookContext& ctx)
			{
				const auto screenWidth = s_instance_->m_newResX;
				const auto screenHeight = s_instance_->m_newResY;

				if (screenWidth == 0 || screenHeight == 0)
				{
					return;
				}

				auto* const stack = reinterpret_cast<std::uintptr_t*>(ctx.esp);
				const uintptr_t binkHandle = *reinterpret_cast<const std::uintptr_t*>(Memory::GetPointerFromAddress<uintptr_t>(s_instance_->m_resolutionScansResult[BinkVideoHandle] + 1, Memory::PointerMode::Absolute));

				if (binkHandle == 0)
				{
					return;
				}

				const auto videoWidth = *reinterpret_cast<const std::uint32_t*>(binkHandle);
				const auto videoHeight = *reinterpret_cast<const std::uint32_t*>(binkHandle + 4);

				constexpr std::uint32_t maximumDimension = 16384;

				if (videoWidth == 0 || videoHeight == 0 || videoWidth > maximumDimension || videoHeight > maximumDimension)
				{
					return;
				}

				const double scaleX = static_cast<double>(screenWidth) / static_cast<double>(videoWidth);
				const double scaleY = static_cast<double>(screenHeight) / static_cast<double>(videoHeight);
				const double scale = std::min(scaleX, scaleY);
				const LONG outputWidth = static_cast<LONG>(std::lround(static_cast<double>(videoWidth) * scale));
				const LONG outputHeight = static_cast<LONG>(std::lround(static_cast<double>(videoHeight) * scale));

				if (outputWidth <= 0 || outputHeight <= 0)
				{
					return;
				}

				const LONG screenWidthLong = static_cast<LONG>(screenWidth);
				const LONG screenHeightLong = static_cast<LONG>(screenHeight);

				RECT& rect = s_instance_->m_binkVideoRect;
				rect.left = (screenWidthLong - outputWidth) / 2;
				rect.top = (screenHeightLong - outputHeight) / 2;
				rect.right = rect.left + outputWidth;
				rect.bottom = rect.top + outputHeight;

				stack[1] = reinterpret_cast<std::uintptr_t>(&rect);
			});
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "DC 3D ?? ?? ?? ?? D9 51", "A1 ?? ?? ?? ?? 56 57 8B F9", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B CE 68");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Outside Views Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[OutsideViews] - (std::uint8_t*)ExeModule());
			spdlog::info("Cockpit Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[Cockpit] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_cameraFOVScansResult[General], 6);

			m_generalFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newOverallFOV = 1.0 / (double)s_instance_->m_aspectRatioScale;
				FPU::FDIVR(s_instance_->m_newOverallFOV);
			});

			m_outsideViewsFOVAddress = Memory::GetPointerFromAddress(m_cameraFOVScansResult[OutsideViews] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(m_cameraFOVScansResult[OutsideViews], 5);

			m_outsideViewsFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[OutsideViews], [](SafetyHookContext& ctx)
			{
				float& fCurrentOutsideViewsFOV = Memory::ReadMem(s_instance_->m_outsideViewsFOVAddress);
				s_instance_->m_newOutsideViewsFOV = fCurrentOutsideViewsFOV * s_instance_->m_fovFactor;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newOutsideViewsFOV);
			});

			m_newCockpitFOV = m_originalCockpitFOV * m_fovFactor;

			Memory::Write(m_cameraFOVScansResult[Cockpit] + 1, m_newCockpitFOV);
		}

		m_hudScansResult = Memory::PatternScan(ExeModule(), "E8 ?? ?? ?? ?? 5D 5B 5F 5E 81 C4", "39 35 ?? ?? ?? ?? 0f 84 ?? ?? ?? ?? 8b 3d",
		"A1 ?? ?? ?? ?? 83 EC 18 55 56 33 F6 57 3B C6 8B FA 8B E9 75 1E 68 ?? ?? ?? ?? 68 DE 03 00 00");
		if (Memory::AreAllSignaturesValid(m_hudScansResult) == true)
		{
			spdlog::info("Font Quad Aspect Ratio Hook: Address is {:s}+{:x}", ExeName().c_str(), m_hudScansResult[FontQuad] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Menu Backgrounds Hook: Address is {:s}+{:x}", ExeName().c_str(), m_hudScansResult[MenuBackgrounds] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Races HUD Hook: Address is {:s}+{:x}", ExeName().c_str(), m_hudScansResult[Races] - reinterpret_cast<std::uint8_t*>(ExeModule()));

			m_fontQuadHook = safetyhook::create_mid(m_hudScansResult[FontQuad], [](SafetyHookContext & ctx)
			{
				if (!std::isfinite(s_instance_->m_aspectRatioScale) || s_instance_->m_aspectRatioScale <= 0.0f)
				{
					return;
				}

				if (std::abs(s_instance_->m_aspectRatioScale - 1.0f) < 0.0001f)
				{
					return;
				}

				auto& x1 = *reinterpret_cast<float*>(ctx.esp + 0x38);
				auto& x2 = *reinterpret_cast<float*>(ctx.esp + 0x58);
				auto& x3 = *reinterpret_cast<float*>(ctx.esp + 0x78);
				auto& x4 = *reinterpret_cast<float*>(ctx.esp + 0x98);

				if (!std::isfinite(x1) || !std::isfinite(x2) || !std::isfinite(x3) || !std::isfinite(x4))
				{
					return;
				}

				static const float screenCenterX = static_cast<float>(s_instance_->m_newResX) * 0.5f;

				const auto correctX = [](float x)
				{
					return screenCenterX + ((x - screenCenterX) / s_instance_->m_aspectRatioScale);
				};

				const float oldX1 = x1;
				const float oldX2 = x2;
				const float oldX3 = x3;
				const float oldX4 = x4;

				x1 = correctX(x1);
				x2 = correctX(x2);
				x3 = correctX(x3);
				x4 = correctX(x4);
			});

			m_menuBackgroundsHook = safetyhook::create_mid(m_hudScansResult[MenuBackgrounds], [](SafetyHookContext & ctx)
			{
				const auto moduleBase = reinterpret_cast<std::uintptr_t>(s_instance_->ExeModule());
				const auto directBackgroundActive = *reinterpret_cast<const std::uint32_t*>(moduleBase + 0x1355D8);

				if (directBackgroundActive == 0)
				{
					return;
				}

				const auto screenWidth = static_cast<std::uint32_t>(*reinterpret_cast<const std::uint16_t*>(moduleBase + 0x1D2096));
				const auto screenHeight = static_cast<std::uint32_t>(*reinterpret_cast<const std::uint16_t*>(moduleBase + 0x1D2098));

				if (screenWidth == 0 || screenHeight == 0)
				{
					return;
				}

				constexpr float nativeAspectRatio = 4.0f / 3.0f;
				const float currentAspectRatio = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
				static const float aspectRatioScale = currentAspectRatio / nativeAspectRatio;

				if (!std::isfinite(aspectRatioScale) || aspectRatioScale <= 0.0f || std::abs(aspectRatioScale - 1.0f) < 0.0001f)
				{
					return;
				}

				auto& x1 = *reinterpret_cast<float*>(moduleBase + 0x1355F8);
				auto& x2 = *reinterpret_cast<float*>(moduleBase + 0x135618);
				auto& x3 = *reinterpret_cast<float*>(moduleBase + 0x135638);
				auto& x4 = *reinterpret_cast<float*>(moduleBase + 0x135658);

				if (!std::isfinite(x1) || !std::isfinite(x2) || !std::isfinite(x3) || !std::isfinite(x4))
				{
					return;
				}

				constexpr float tolerance = 1.0f;
				const float screenWidthFloat = static_cast<float>(screenWidth);
				const bool expectedQuad = std::abs(x1) <= tolerance && std::abs(x4) <= tolerance &&
				std::abs(x2 - screenWidthFloat) <= tolerance &&
				std::abs(x3 - screenWidthFloat) <= tolerance;
				if (!expectedQuad)
				{
					return;
				}

				static const float centerX = screenWidthFloat * 0.5f;

				const auto correctX = [](const float x)
				{
					return centerX + ((x - centerX) / aspectRatioScale);
				};

				x1 = correctX(x1);
				x2 = correctX(x2);
				x3 = correctX(x3);
				x4 = correctX(x4);
			});

				m_raceHUDHook = safetyhook::create_mid(m_hudScansResult[Races], [](SafetyHookContext & ctx)
				{
					const auto* const instance = s_instance_;

					if (instance == nullptr || ctx.ecx == 0)
					{
						return;
					}

					const uintptr_t moduleBase = reinterpret_cast<std::uintptr_t>(instance->ExeModule());
					const uintptr_t returnAddress = *reinterpret_cast<const std::uintptr_t*>(ctx.esp);

					if (returnAddress < moduleBase)
					{
						return;
					}

					const auto callerRva = returnAddress - moduleBase;

					switch (callerRva)
					{
						// Startup, menus, setup, results and other interfaces
						case 0x6DD24:
						case 0x6DA3E:
						case 0x70C8B:
						case 0x70CF3:
						case 0x70D9F:
						case 0x70DDF:
						case 0x70E1F:
						case 0x70E5D:
						case 0x70E9A:
						case 0x77DE7:

						// Race HUD and g_draw.c interface							
						case 0x5AC00:
						case 0x5C965:
						case 0x5CDED:
						case 0x5DA2A:
						case 0x60AA4:
						case 0x60B3B:
						case 0x60B77:
						case 0x60DE1:
						case 0x6E80E:
							break;

						default:
							return;
					}

					const auto screenWidth = instance->m_newResX;
					const auto screenHeight = instance->m_newResY;

					if (screenWidth == 0 || screenHeight == 0)
					{
						return;
					}

					const float aspectRatioScale = instance->m_aspectRatioScale;

					if (!std::isfinite(aspectRatioScale) || aspectRatioScale <= 0.0f || std::abs(aspectRatioScale - 1.0f) < 0.0001f)
					{
						return;
					}

					struct GfxRectangle
					{
						std::int16_t x;
						std::int16_t y;
						std::uint16_t width;
						std::uint16_t height;
					};

					auto* const rectangle = reinterpret_cast<GfxRectangle*>(static_cast<std::uintptr_t>(ctx.ecx));
					const float oldLeft = static_cast<float>(rectangle->x);
					const float oldRight = oldLeft + static_cast<float>(rectangle->width);

					if (!std::isfinite(oldLeft) || !std::isfinite(oldRight) || rectangle->width == 0 || rectangle->height == 0)
					{
						return;
					}

					const float screenCenterX = static_cast<float>(screenWidth) * 0.5f;
					const float newLeft = screenCenterX + ((oldLeft - screenCenterX) / aspectRatioScale);
					const float newRight = screenCenterX + ((oldRight - screenCenterX) / aspectRatioScale);
					const float newWidth = newRight - newLeft;

					if (!std::isfinite(newLeft) || !std::isfinite(newWidth) || newWidth <= 0.0f)
					{
						return;
					}

					constexpr float signedMinimum = static_cast<float>(std::numeric_limits<std::int16_t>::min());
					constexpr float signedMaximum = static_cast<float>(std::numeric_limits<std::int16_t>::max());
					constexpr float unsignedMaximum = static_cast<float>(std::numeric_limits<std::uint16_t>::max());
					const float clampedLeft = std::clamp(newLeft, signedMinimum, signedMaximum);
					const float clampedWidth = std::clamp(newWidth, 1.0f, unsignedMaximum);
					const auto oldX = rectangle->x;
					const auto oldWidth = rectangle->width;
					rectangle->x = static_cast<std::int16_t>(std::lround(clampedLeft));
					rectangle->width = static_cast<std::uint16_t>(std::lround(clampedWidth));
				});
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "75 ?? 5F 5E 5D B8 ?? ?? ?? ?? 5B 81 C4 ?? ?? ?? ?? C3");
			if (m_skipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_skipIntroVideosScanResult, 2);
			}
			else
			{
				spdlog::error("Failed to locate skip intro check instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCockpitFOV = 80.0f;
	static constexpr float m_originalHUDWidth = 320;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::vector<std::uint8_t*> m_hudScansResult{};
	std::uint8_t* m_skipIntroVideosScanResult = nullptr;

	bool m_skipIntroVideos = false;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_binkVideoRectHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_outsideViewsFOVHook{};
	SafetyHookMid m_fontQuadHook{};
	SafetyHookMid m_menuBackgroundsHook{};
	SafetyHookMid m_raceHUDHook{};

	RECT m_binkVideoRect{};

	uintptr_t m_outsideViewsFOVAddress = 0;

	double m_newOverallFOV = 0.0;
	float m_newOutsideViewsFOV = 0.0f;
	float m_newCockpitFOV = 0.0f;

	enum ResolutionInstructionsIndex
	{
		WidthHeight,
		BinkVideoRect,
		BinkVideoHandle
	};

	enum CameraFOVInstructionsIndex
	{
		General,
		OutsideViews,
		Cockpit
	};

	enum HUDInstructionsIndex
	{
		FontQuad,
		MenuBackgrounds,
		Races
	};

	inline static RallyMastersFix* s_instance_ = nullptr;
};

static std::unique_ptr<RallyMastersFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<RallyMastersFix>(hModule);
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