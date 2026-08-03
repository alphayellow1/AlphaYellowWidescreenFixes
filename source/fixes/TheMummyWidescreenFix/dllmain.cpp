#include "..\..\common\FixBase.hpp"

class TheMummyFix final : public FixBase
{
public:
	explicit TheMummyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TheMummyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheMummyWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.7";
	}

	const char* TargetName() const override
	{
		return "The Mummy";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "MummyPC.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "8B 90 ?? ?? ?? ?? 8B 88 ?? ?? ?? ?? 89 15", "FF 51 ?? 8B CB", "A1 ?? ?? ?? ?? 0B D6", "55 C1 E0 ?? 99",
		"3B DD 89 4C 24", "C1 E0 ?? 99 F7 FD 8B 1D", "3B EB 89 4C 24", "DB 44 24 ?? D8 0D ?? ?? ?? ?? D9 1D ?? ?? ?? ?? DB 44 24", 
		"81 EC ?? ?? ?? ?? 8B 84 24 ?? ?? ?? ?? 8B 8C 24", "81 EC ?? ?? ?? ?? DB 84 24 ?? ?? ?? ?? 8B 8C 24",
		"81 EC ?? ?? ?? ?? 8B 41", "66 89 56 ?? 03 C8", "66 8B 54 24 40", "DB 44 24 ?? 0F BF 51", "0F BF 41 ?? D9 54 24");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());
			spdlog::info("Bink Video Rectangle Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkVideoRectangle] - (std::uint8_t*)ExeModule());
			spdlog::info("Bink Video Handle Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkVideoHandle] - (std::uint8_t*)ExeModule());
			spdlog::info("Image Width Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ImageWidth] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Image Destination Row Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ImageDestinationRow] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Loaded Image Width Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[LoadedImageWidth] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Loaded Image Destination Row Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[LoadedImageDestinationRow] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("HUD Scale Initialize Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[HudScaleInitialize] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("HUD Draw Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[HudDraw] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("HUD Quad Draw Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[HudQuadDraw] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("HUD Basic Draw Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[HudBasicDraw] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Text Glyph Coordinates Hook: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[TextGlyphCoordinates] + 0x12 - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Simple Text Glyph Coordinates Hook: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[SimpleTextGlyphCoordinates] + 0x26 - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Audio Slider Left Position Hook: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[DirectHudLeftPosition] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Audio Slider Right Position Hook: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[DirectHudRightPosition] - reinterpret_cast<std::uint8_t*>(ExeModule()));

			m_resolutionWidthOffset = Memory::GetPointerFromAddress(m_resolutionScansResult[WidthHeight] + 8, Memory::PointerMode::Absolute);
			m_resolutionHeightOffset = Memory::GetPointerFromAddress(m_resolutionScansResult[WidthHeight] + 2, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->hitCount++;	

				if (s_instance_->hitCount == 2)
				{
					s_instance_->m_newResX = Memory::ReadMem(ctx.eax + s_instance_->m_resolutionWidthOffset);
					s_instance_->m_newResY = Memory::ReadMem(ctx.eax + s_instance_->m_resolutionHeightOffset);
					s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->WriteStaticFOVs();
					s_instance_->m_resolutionHook.disable();
				}
			});

			s_instance_->m_binkHandleAddress = Memory::GetPointerFromAddress(s_instance_->m_resolutionScansResult[BinkVideoHandle] + 1, Memory::PointerMode::Absolute);

			m_binkVideoRectHook = safetyhook::create_mid(m_resolutionScansResult[BinkVideoRectangle] - 1, [](SafetyHookContext& ctx)
			{
				if (s_instance_->m_binkHandleAddress == 0 || ctx.ebx == 0)
				{	
					return;
				}

				const std::uintptr_t binkHandle = static_cast<std::uintptr_t>(Memory::ReadMem(s_instance_->m_binkHandleAddress));

				if (binkHandle == 0)
				{
					return;
				}

				const std::uint32_t videoWidth = Memory::ReadMem(binkHandle);
				const std::uint32_t videoHeight = Memory::ReadMem(binkHandle + 4);

				if (videoWidth == 0 || videoHeight == 0)
				{
					return;
				}

				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				std::int32_t drawWidth = 0;
				std::int32_t drawHeight = 0;

				if (static_cast<std::int64_t>(screenWidth) * videoHeight <= static_cast<std::int64_t>(screenHeight) * videoWidth)
				{
					drawWidth = screenWidth;
					drawHeight = static_cast<std::int32_t>(static_cast<std::int64_t>(drawWidth) * videoHeight / videoWidth);
				}
				else
				{
					drawHeight = screenHeight;
					drawWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(drawHeight) * videoWidth / videoHeight);
				}

				if (drawWidth <= 0 || drawHeight <= 0)
				{
					return;
				}

				RECT& destinationRect = s_instance_->m_binkDestinationRect;
				destinationRect.left = (screenWidth - drawWidth) / 2;
				destinationRect.top = (screenHeight - drawHeight) / 2;
				destinationRect.right = destinationRect.left + drawWidth;
				destinationRect.bottom = destinationRect.top + drawHeight;

				std::uintptr_t& destinationRectArgument = Memory::ReadMem(ctx.esp);

				destinationRectArgument = reinterpret_cast<std::uintptr_t>(&destinationRect);
			});

			m_imageWidthHook = safetyhook::create_mid(m_resolutionScansResult[ImageWidth], [](SafetyHookContext& ctx)
			{
				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				if (safeWidth <= 0 || safeWidth > screenWidth)
				{
					return;
				}

				ctx.ebx = static_cast<std::uintptr_t>(safeWidth);
			});

			m_imageOffsetHook = safetyhook::create_mid(m_resolutionScansResult[ImageDestinationRow], [](SafetyHookContext& ctx)
			{
				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				const std::int32_t offsetX = (screenWidth - safeWidth) / 2;

				if (offsetX <= 0)
				{
					return;
				}

				const std::int32_t pixelDepth = Memory::ReadMem(ctx.esp + 0x4C);

				std::int32_t bytesPerPixel = 0;

				switch (pixelDepth)
				{
					case 15:
					case 16:
						bytesPerPixel = 2;
						break;

					case 24:
						bytesPerPixel = 3;
						break;

					case 32:
						bytesPerPixel = 4;
						break;

					default:
						return;
				}

				ctx.ecx += static_cast<std::uintptr_t>(offsetX * bytesPerPixel);
			});

			m_loadedImageWidthHook = safetyhook::create_mid(m_resolutionScansResult[LoadedImageWidth], [](SafetyHookContext & ctx)
			{
				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				if (safeWidth <= 0 || safeWidth > screenWidth)
				{
					return;
				}

				ctx.ebp = static_cast<std::uintptr_t>(safeWidth);
			});

			m_loadedImageOffsetHook = safetyhook::create_mid(m_resolutionScansResult[LoadedImageDestinationRow], [](SafetyHookContext & ctx)
			{
				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				const std::int32_t offsetX = (screenWidth - safeWidth) / 2;

				if (offsetX <= 0)
				{
					return;
				}

				const std::int32_t pixelDepth = Memory::ReadMem(ctx.esp + 0x4C);

				std::int32_t bytesPerPixel = 0;

				switch (pixelDepth)
				{
					case 15:
					case 16:
						bytesPerPixel = 2;
						break;

					case 24:
						bytesPerPixel = 3;
						break;

					case 32:
						bytesPerPixel = 4;
						break;

					default:
						return;
				}

				ctx.ecx += static_cast<std::uintptr_t>(offsetX * bytesPerPixel);
			});

			m_hudScaleInitializeHook = safetyhook::create_mid(m_resolutionScansResult[HudScaleInitialize], [](SafetyHookContext & ctx)
			{
				const std::int32_t screenWidth = Memory::ReadMem(ctx.esp + 0x4);
				const std::int32_t screenHeight = Memory::ReadMem(ctx.esp + 0x8);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				if (safeWidth <= 0 || safeWidth > screenWidth)
				{
					return;
				}

				s_instance_->m_hudPhysicalOffsetX = static_cast<float>((screenWidth - safeWidth) / 2);

				s_instance_->m_newResX = screenWidth;
				s_instance_->m_newResY = screenHeight;

				s_instance_->m_newAspectRatio = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;

				Memory::Write(ctx.esp + 0x04, safeWidth);

				s_instance_->m_hudScaleApplied = true;
			});

			m_hudPositionHook = safetyhook::create_mid(m_resolutionScansResult[HudDraw], [](SafetyHookContext & ctx)
			{
				if (!s_instance_->m_hudScaleApplied)
				{
					return;
				}

				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				if (safeWidth <= 0 || safeWidth >= screenWidth)
				{
					return;
				}

				const std::int32_t originalX = Memory::ReadMem(ctx.esp + 0x04);
				const std::int32_t width = Memory::ReadMem(ctx.esp + 0x0C);
				const std::int32_t height = Memory::ReadMem(ctx.esp + 0x10);

				std::int64_t absoluteWidth = 0;

				if (width < 0)
				{
					absoluteWidth = -static_cast<std::int64_t>(width);
				}
				else
				{
					absoluteWidth = static_cast<std::int64_t>(width);
				}

				std::int64_t absoluteHeight = 0;

				if (height < 0)
				{
					absoluteHeight = -static_cast<std::int64_t>(height);
				}
				else
				{
					absoluteHeight = static_cast<std::int64_t>(height);
				}

				if (absoluteWidth == 0 || absoluteHeight == 0)
				{
					return;
				}

				const std::int32_t offsetX = (screenWidth - safeWidth) / 2;

				std::int32_t correctedX = originalX;

				correctedX += offsetX;

				if (correctedX != originalX)
				{
					Memory::Write(ctx.esp + 0x4, correctedX);
				}
			});

			m_hudQuadPositionHook = safetyhook::create_mid(m_resolutionScansResult[HudQuadDraw], [](SafetyHookContext & ctx)
			{
				if (!s_instance_->m_hudScaleApplied)
				{
					return;
				}

				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				if (safeWidth <= 0 || safeWidth >= screenWidth)
				{
					return;
				}

				std::int32_t x1 = Memory::ReadMem(ctx.esp + 0x4);
				std::int32_t x2 = Memory::ReadMem(ctx.esp + 0xC);
				std::int32_t x3 = Memory::ReadMem(ctx.esp + 0x14);
				std::int32_t x4 = Memory::ReadMem(ctx.esp + 0x1C);

				const std::int32_t minimumX = std::min(std::min(x1, x2), std::min(x3, x4));
				const std::int32_t maximumX = std::max(std::max(x1, x2), std::max(x3, x4));
				const std::int64_t primitiveWidth = static_cast<std::int64_t>(maximumX) - static_cast<std::int64_t>(minimumX);

				if (primitiveWidth < 0)
				{
					return;
				}

				const std::int32_t offsetX = (screenWidth - safeWidth) / 2;

				std::int32_t translationX = 0;

				translationX = offsetX;

				if (translationX == 0)
				{
					return;
				}

				x1 += translationX;
				x2 += translationX;
				x3 += translationX;
				x4 += translationX;

				Memory::Write(ctx.esp + 0x4, x1);
				Memory::Write(ctx.esp + 0xC, x2);
				Memory::Write(ctx.esp + 0x14, x3);
				Memory::Write(ctx.esp + 0x1C, x4);
			});

			m_hudBasicPositionHook = safetyhook::create_mid(m_resolutionScansResult[HudBasicDraw], [](SafetyHookContext & ctx)
			{
				if (!s_instance_->m_hudScaleApplied)
				{
					return;
				}

				const std::int32_t screenWidth = static_cast<std::int32_t>(s_instance_->m_newResX);
				const std::int32_t screenHeight = static_cast<std::int32_t>(s_instance_->m_newResY);

				if (screenWidth <= 0 || screenHeight <= 0)
				{
					return;
				}

				const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

				if (safeWidth <= 0 || safeWidth >= screenWidth)
				{
					return;
				}

				const std::int32_t originalX = Memory::ReadMem(ctx.esp + 0x4);
				const std::int32_t width = Memory::ReadMem(ctx.esp + 0xC);
				const std::int32_t height = Memory::ReadMem(ctx.esp + 0x10);

				std::int64_t absoluteWidth = 0;

				if (width < 0)
				{
					absoluteWidth = -static_cast<std::int64_t>(width);
				}
				else
				{
					absoluteWidth = static_cast<std::int64_t>(width);
				}

				std::int64_t absoluteHeight = 0;

				if (height < 0)
				{
					absoluteHeight = -static_cast<std::int64_t>(height);
				}
				else
				{
					absoluteHeight = static_cast<std::int64_t>(height);
				}

				if (absoluteWidth == 0 || absoluteHeight == 0 || absoluteWidth >= safeWidth || absoluteHeight >= screenHeight)
				{
					return;
				}

				const std::int32_t offsetX = (screenWidth - safeWidth) / 2;

				std::int32_t correctedX = originalX;

				correctedX += offsetX;

				if (correctedX != originalX)
				{
					Memory::Write(ctx.esp + 0x4, correctedX);
				}
			});

			m_directHudLeftPositionHook = safetyhook::create_mid(m_resolutionScansResult[DirectHudLeftPosition], [](SafetyHookContext & ctx)
			{
				if (!s_instance_->m_hudScaleApplied ||  ctx.ecx == 0)
				{
					return;
				}

				if (!s_instance_->IsOptionsDirectHudObject(ctx.ecx))
				{
					return;
				}

				FPU::FADD(s_instance_->m_hudPhysicalOffsetX);
			});

			m_directHudRightPositionHook = safetyhook::create_mid(m_resolutionScansResult[DirectHudRightPosition], [](SafetyHookContext& ctx)
			{
				if (!s_instance_->m_hudScaleApplied || ctx.ecx == 0)
				{
					return;
				}

				if (!s_instance_->IsOptionsDirectHudObject(ctx.ecx))
				{
					return;
				}

				FPU::FADD(s_instance_->m_hudPhysicalOffsetX);
			});

			m_textGlyphCoordinatesHook = safetyhook::create_mid(m_resolutionScansResult[TextGlyphCoordinates] + 18, [](SafetyHookContext & ctx)
			{
				s_instance_->TranslateTextGlyphVertices(ctx);
			});

			m_simpleTextGlyphCoordinatesHook = safetyhook::create_mid(m_resolutionScansResult[SimpleTextGlyphCoordinates] + 38, [](SafetyHookContext & ctx)
			{
				s_instance_->TranslateTextGlyphVertices(ctx);
			});
		}

		m_aspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 89 44 24 ?? 8B 42");
		if (m_aspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_aspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(m_aspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? A3", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? E8 ?? ?? ?? ?? 8B CE");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("Cutscenes Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[Cutscenes] - (std::uint8_t*)ExeModule());
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());
		}

		if (m_runMultipleInstances == true)
		{
			m_multipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? 56 8B 35");
			if (m_multipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_multipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_multipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "6A ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68",
			"6A ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 59 C3 90 A1", "E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B F0", "68 ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? E8");
			if (Memory::AreAllSignaturesValid(m_skipIntroVideosScansResult) == true)
			{
				spdlog::info("Skip Intro Videos: startup logo FMV sequence found at {:s}+{:x}", ExeName(), m_skipIntroVideosScansResult[StartupLogoSequence] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos: ED_book_hi.bik playback found at {:s}+{:x}", ExeName(), m_skipIntroVideosScansResult[BookIntro] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos: license1 image loader call found at {:s}+{:x}", ExeName(), m_skipIntroVideosScansResult[LicenseImageLoader] - (std::uint8_t*)ExeModule());
				spdlog::info("Skip Intro Videos: license image display duration found at {:s}+{:x}", ExeName(), m_skipIntroVideosScansResult[LicenseDisplayDelay] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_skipIntroVideosScansResult[StartupLogoSequence], "\xEB\x31");
				Memory::PatchBytes(m_skipIntroVideosScansResult[BookIntro], "\xEB\x0F");
				Memory::WriteNOPs(m_skipIntroVideosScansResult[LicenseImageLoader], 5);
				Memory::Write(m_skipIntroVideosScansResult[LicenseDisplayDelay] + 1, m_skippedDisplayDuration);
			}
		}		
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr int m_iOriginalCutscenesFOV = 1024;
	static constexpr int m_iOriginalGameplayFOV = 683;
	static constexpr float m_skippedDisplayDuration = 0.000001f;

	float m_hudPhysicalOffsetX = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_binkVideoRectHook{};
	SafetyHookMid m_imageWidthHook{};
	SafetyHookMid m_imageOffsetHook{};
	SafetyHookMid m_loadedImageWidthHook{};
	SafetyHookMid m_loadedImageOffsetHook{};
	SafetyHookMid m_hudPositionHook{};
	SafetyHookMid m_hudScaleInitializeHook{};
	SafetyHookMid m_hudQuadPositionHook{};
	SafetyHookMid m_hudBasicPositionHook{};
	SafetyHookMid m_textGlyphCoordinatesHook{};
	SafetyHookMid m_simpleTextGlyphCoordinatesHook{};
	SafetyHookMid m_directHudLeftPositionHook{};
	SafetyHookMid m_directHudRightPositionHook{};
	SafetyHookMid m_optionsMenuDiagnosticHook{};
	SafetyHookMid m_aspectRatioHook{};

	std::vector<std::uint8_t*> m_resolutionScansResult = {};
	uint8_t* m_aspectRatioScanResult = nullptr;
	std::vector<uint8_t*> m_cameraFOVScansResult;
	uint8_t* m_multipleInstancesCheckScanResult = nullptr;
	std::vector<uint8_t*> m_skipIntroVideosScansResult = {};

	uintptr_t m_resolutionWidthOffset = 0;
	uintptr_t m_resolutionHeightOffset = 0;
	uintptr_t m_binkHandleAddress = 0;
	RECT m_binkDestinationRect{};

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;
	bool m_hudScaleApplied = false;
	bool m_optionsMenuDetected = false;

	float m_fOriginalCutscenesFOV = 0.0f;
	float m_fNewCutscenesFOV = 0.0f;
	int m_iNewCutscenesFOV = 0;
	float m_fOriginalGameplayFOV = 0.0f;
	float m_fNewGameplayFOV = 0.0f;
	int m_iNewGameplayFOV = 0;

	int hitCount = 0;

	enum ResolutionInstructionsIndex
	{
		WidthHeight,
		BinkVideoRectangle,
		BinkVideoHandle,
		ImageWidth,
		ImageDestinationRow,
		LoadedImageWidth,
		LoadedImageDestinationRow,
		HudScaleInitialize,
		HudDraw,
		HudQuadDraw,
		HudBasicDraw,
		TextGlyphCoordinates,
		SimpleTextGlyphCoordinates,
		DirectHudLeftPosition,
		DirectHudRightPosition
	};

	enum CameraFOVInstructionsIndex
	{
		Cutscenes,
		Gameplay
	};

	enum SkipIntroLogosIndex
	{
		StartupLogoSequence,
		BookIntro,
		LicenseImageLoader,
		LicenseDisplayDelay
	};

	void TranslateTextGlyphVertices(SafetyHookContext& ctx) const
	{
		if (!m_hudScaleApplied || ctx.esi == 0)
		{
			return;
		}

		const std::int32_t screenWidth = static_cast<std::int32_t>(m_newResX);
		const std::int32_t screenHeight = static_cast<std::int32_t>(m_newResY);

		if (screenWidth <= 0 || screenHeight <= 0)
		{
			return;
		}

		const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

		if (safeWidth <= 0 || safeWidth >= screenWidth)
		{
			return;
		}

		const std::int32_t physicalOffsetX = (screenWidth - safeWidth) / 2;
		const std::int32_t virtualOffsetX = static_cast<std::int32_t>((static_cast<std::int64_t>(physicalOffsetX) * 512 + safeWidth / 2) / safeWidth);
		const std::int16_t originalX = Memory::ReadMem(ctx.esi + 0x8);
		const std::int32_t correctedX = static_cast<std::int32_t>(originalX) + virtualOffsetX;

		if (correctedX < std::numeric_limits<std::int16_t>::min() || correctedX > std::numeric_limits<std::int16_t>::max())
		{
			return;
		}

		Memory::Write(ctx.esi + 0x8, static_cast<std::int16_t>(correctedX));
	}

	std::int32_t GetVirtualHudOffsetX() const
	{
		const std::int32_t screenWidth = static_cast<std::int32_t>(m_newResX);
		const std::int32_t screenHeight = static_cast<std::int32_t>(m_newResY);

		if (screenWidth <= 0 || screenHeight <= 0)
		{
			return 0;
		}

		const std::int32_t safeWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(screenHeight) * 4 / 3);

		if (safeWidth <= 0 || safeWidth >= screenWidth)
		{
			return 0;
		}

		const std::int32_t physicalOffsetX = (screenWidth - safeWidth) / 2;

		return static_cast<std::int32_t>((static_cast<std::int64_t>(physicalOffsetX) * 512 + safeWidth / 2) / safeWidth);
	}

	bool IsOptionsDirectHudObject(std::uintptr_t object) const
	{
		if (object == 0)
		{
			return false;
		}

		const std::int16_t x = Memory::ReadMem(object + 0x08);
		const std::int16_t y = Memory::ReadMem(object + 0x0A);
		const std::int16_t width = Memory::ReadMem(object + 0x10);
		const std::int16_t height = Memory::ReadMem(object + 0x12);
		const std::uint8_t colorR = Memory::ReadMem(object + 0x04);
		const std::uint8_t colorG = Memory::ReadMem(object + 0x05);
		const std::uint8_t colorB = Memory::ReadMem(object + 0x06);
		const std::uint8_t value0C = Memory::ReadMem(object + 0x0C);
		const std::uint8_t value0D = Memory::ReadMem(object + 0x0D);

		const bool isSliderFrame = x == 347 && (y == 93 || y == 124) && width == 125 && height == 26;

		if (isSliderFrame == true)
		{
			return true;
		}

		const bool isSliderFill = x == 353 && (y == 100 || y == 131) && width > 0 && width <= 113 && height == 12;

		if (isSliderFill == true)
		{
			return true;
		}

		const bool isAdjustmentArrow = (x == 296 || x == 318) && y == 208 && width == 20 && height == 12 && colorR == 0 &&
		colorG == 120 &&colorB == 240 && value0C == 234 && (value0D == 1 || value0D == 15);

		return isAdjustmentArrow;
	}

	void WriteStaticFOVs()
	{
		m_fOriginalCutscenesFOV = (float)m_iOriginalCutscenesFOV * 360.0f / 4096.0f;
		m_fNewCutscenesFOV = Maths::CalculateNewFOV_DegBased(m_fOriginalCutscenesFOV, m_aspectRatioScale);
		m_iNewCutscenesFOV = (int)(m_fNewCutscenesFOV * 4096.0f / 360.0f);

		m_fOriginalGameplayFOV = (float)m_iOriginalGameplayFOV * 360.0f / 4096.0f;
		m_fNewGameplayFOV = Maths::CalculateNewFOV_DegBased(m_fOriginalGameplayFOV, m_aspectRatioScale) * m_fovFactor;
		m_iNewGameplayFOV = (int)(m_fNewGameplayFOV * 4096.0f / 360.0f);

		Memory::Write(m_cameraFOVScansResult[Cutscenes] + 1, m_iNewCutscenesFOV);
		Memory::Write(m_cameraFOVScansResult[Gameplay] + 1, m_iNewGameplayFOV);
	}

	inline static TheMummyFix* s_instance_ = nullptr;
};

static std::unique_ptr<TheMummyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<TheMummyFix>(hModule);
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