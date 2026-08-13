#include "..\..\common\FixBase.hpp"

class BackyardBaseball2005Fix final : public FixBase
{
public:
	explicit BackyardBaseball2005Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BackyardBaseball2005Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BackyardBaseball2005WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Backyard Baseball 2005";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Baseball.exe") ||
		Util::stringcmp_caseless(exeName, "BaseballSR.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		inipp::get_value(ini.sections["Settings"], "SkipIntroLogos", m_skipIntroLogos);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
		spdlog_confparse(m_skipIntroLogos);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "B8 ?? ?? ?? ?? 89 45 ?? BA", "B8 ?? ?? ?? ?? 89 45 ?? B9",
		"FF 15 ?? ?? ?? ?? 5E C2 ?? ?? CC CC CC CC CC CC CC CC CC CC 6A", "8B 4F ?? B8 ?? ?? ?? ?? 3B C8 B3");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[Res1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[Res2] - (std::uint8_t*)ExeModule());
			spdlog::info("BinkCopyToBuffer Call Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkCopyToBuffer] - (std::uint8_t*)ExeModule());
			spdlog::info("Bink Videos/Intro Images Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[Bink_Images_Rect] - (std::uint8_t*)ExeModule());

			Memory::Write(m_resolutionScansResult[Res1] + 1, m_newResX);
			Memory::Write(m_resolutionScansResult[Res1] + 9, m_newResY);
			Memory::Write(m_resolutionScansResult[Res2] + 1, m_newResX);
			Memory::Write(m_resolutionScansResult[Res2] + 9, m_newResY);

			s_originalBinkCopyToBuffer = *reinterpret_cast<BinkCopyToBufferFn*>(reinterpret_cast<std::uintptr_t>(ExeModule()) + 0x1FB3D0);
			s_binkCopyToBufferTarget = reinterpret_cast<void*>(&BinkCopyToBufferHook);
			Memory::Write(m_resolutionScansResult[BinkCopyToBuffer] + 2, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&s_binkCopyToBufferTarget)));	

			m_uiContainerAspectHook = safetyhook::create_mid(m_resolutionScansResult[Bink_Images_Rect], [](SafetyHookContext& ctx)
			{
				if (s_instance_ == nullptr || ctx.edi == 0)
				{
					return;
				}

				const auto returnAddress = *reinterpret_cast<const std::uintptr_t*>(ctx.esp + 0x14);
				const auto moduleBase = reinterpret_cast<std::uintptr_t>(s_instance_->ExeModule());

				if (returnAddress < moduleBase || returnAddress >= moduleBase + 0x300000)
				{
					return;
				}

				const auto callerOffset = returnAddress - moduleBase;
				const bool shouldCorrect = callerOffset == 0xC8E66 || callerOffset == 0xC578D || callerOffset == 0x6FBE5;

				if (!shouldCorrect)
				{
					return;
				}

				const float currentAspectRatio = s_instance_->m_newAspectRatio;

				if (currentAspectRatio <= 0.0f)
				{
					return;
				}

				const float horizontalScale = std::min(m_oldAspectRatio / currentAspectRatio, 1.0f);
				*reinterpret_cast<float*>(ctx.edi + 0x18) = 320.0f;
				*reinterpret_cast<float*>(ctx.edi + 0x1C) = 240.0f;
				*reinterpret_cast<float*>(ctx.edi + 0x20) = horizontalScale;
				*reinterpret_cast<float*>(ctx.edi + 0x24) = 1.0f;
			});
		}

		m_aspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 50 51 8B 0E D9 1C ?? E8 ?? ?? ?? ?? 8B 4C 24 ?? 89 4E ?? 5E");
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

		m_cameraFOVScanResult = Memory::PatternScan(ExeModule(), "DC 0D ?? ?? ?? ?? D9 5C 24 08 8B 44 24 08 D9 44 24 08");
		if (m_cameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = 0.75 * (double)m_fovFactor;

			Memory::WriteNOPs(m_cameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(m_cameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			m_multipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? 56 6A ?? 68");
			if (m_multipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_multipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_multipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check scan memory address.");
				return;
			}
		}

		if (m_skipIntroLogos == true)
		{
			m_skipIntroLogosScanResult = Memory::PatternScan(ExeModule(), "A1 ?? ?? ?? ?? 8D 04 40 C1 E0");
			if (m_skipIntroLogosScanResult)
			{
				spdlog::info("Skip Intro Logos Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroLogosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_skipIntroLogosScanResult, "\xE9\xFA\x01\x00\x00");
			}
			else
			{
				spdlog::error("Failed to locate skip intro logos check instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::uint8_t* m_cameraFOVScanResult{};
	std::uint8_t* m_aspectRatioScanResult{};
	std::uint8_t* m_multipleInstancesCheckScanResult{};
	std::uint8_t* m_skipIntroLogosScanResult{};

	bool m_runMultipleInstances = false;
	bool m_skipIntroLogos = false;

	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};
	SafetyHookMid m_uiContainerAspectHook{};

	double m_newCameraFOV = 0.0;

	enum ResolutionInstructionsIndex
	{
		Res1,
		Res2,
		BinkCopyToBuffer,
		Bink_Images_Rect
	};
        
    struct BinkHandleHeader
    {
        std::uint32_t width;
        std::uint32_t height;
    };

    using BinkCopyToBufferFn =
    std::int32_t(__stdcall*)(
        void* bink,
        void* destination,
        std::int32_t destinationPitch,
        std::uint32_t destinationHeight,
        std::uint32_t destinationX,
        std::uint32_t destinationY,
        std::uint32_t flags);

	inline static BinkCopyToBufferFn s_originalBinkCopyToBuffer = nullptr;

	inline static void* s_binkCopyToBufferTarget = nullptr;
    inline static std::uint32_t s_binkLogCount = 0;

	inline static thread_local std::vector<std::uint32_t> s_binkDecodeBuffer{};

	inline static bool s_loggedBinkScaling = false;

    static std::int32_t __stdcall BinkCopyToBufferHook(void* bink, void* destination, std::int32_t destinationPitch, std::uint32_t destinationHeight,
    std::uint32_t destinationX, std::uint32_t destinationY, std::uint32_t flags)
    {
        if (s_originalBinkCopyToBuffer == nullptr)
        {
            return 0;
        }

        if (bink == nullptr || destination == nullptr || destinationPitch == 0 || destinationHeight == 0)
        {
            return s_originalBinkCopyToBuffer(bink, destination, destinationPitch, destinationHeight, destinationX, destinationY, flags);
        }

        const auto* header = static_cast<const BinkHandleHeader*>(bink);
        const std::uint32_t sourceWidth = header->width;
        const std::uint32_t sourceHeight = header->height;

        if (sourceWidth == 0 || sourceHeight == 0 || sourceWidth > 8192 || sourceHeight > 8192)
        {
            return s_originalBinkCopyToBuffer(bink, destination, destinationPitch, destinationHeight, destinationX, destinationY, flags);
        }

        const std::uint32_t surfaceFormat = flags & 0xFF;

        if (surfaceFormat != 3)
        {
            return s_originalBinkCopyToBuffer(bink, destination, destinationPitch, destinationHeight, destinationX, destinationY, flags);
        }

        constexpr std::uint32_t bytesPerPixel = 4;

        const std::uint32_t absolutePitch = destinationPitch < 0 ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(destinationPitch)) : static_cast<std::uint32_t>(destinationPitch);

        if (absolutePitch < bytesPerPixel)
        {
            return s_originalBinkCopyToBuffer(bink, destination, destinationPitch, destinationHeight, destinationX, destinationY, flags);
        }

        const std::uint32_t destinationWidth = absolutePitch / bytesPerPixel;

        if (destinationWidth == 0)
        {
			return s_originalBinkCopyToBuffer(bink, destination, destinationPitch, destinationHeight, destinationX, destinationY, flags);
        }

        const std::size_t sourcePixelCount = static_cast<std::size_t>(sourceWidth) * static_cast<std::size_t>(sourceHeight);
        constexpr std::size_t maximumPixelCount = static_cast<std::size_t>(8192) * 8192;

        if (sourcePixelCount == 0 || sourcePixelCount > maximumPixelCount)
        {
			return s_originalBinkCopyToBuffer(bink, destination, destinationPitch, destinationHeight, destinationX, destinationY, flags);
        }

        s_binkDecodeBuffer.resize(sourcePixelCount);
        const std::uint64_t temporaryPitch64 = static_cast<std::uint64_t>(sourceWidth) * bytesPerPixel;

        if (temporaryPitch64 > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
        {
			return s_originalBinkCopyToBuffer(bink, destination, destinationPitch, destinationHeight, destinationX, destinationY, flags);
        }

        const auto temporaryPitch = static_cast<std::int32_t>(temporaryPitch64);
        const std::int32_t result = s_originalBinkCopyToBuffer(bink, s_binkDecodeBuffer.data(), temporaryPitch, sourceHeight, 0, 0, flags);
        const std::uint32_t scaledHeight = destinationHeight;
        const std::uint32_t scaledWidth = static_cast<std::uint32_t>(static_cast<std::uint64_t>(sourceWidth) * scaledHeight / sourceHeight);

        if (scaledWidth == 0 || scaledHeight == 0)
        {
            return result;
        }

        const std::int64_t destinationLeft = (static_cast<std::int64_t>(destinationWidth) - static_cast<std::int64_t>(scaledWidth)) / 2;
		auto* destinationBytes = static_cast<std::uint8_t*>(destination);

        for (std::uint32_t outputY = 0; outputY < destinationHeight; ++outputY)
        {
            std::uint8_t* destinationRow = nullptr;

            if (destinationPitch >= 0)
            {
                destinationRow = destinationBytes + static_cast<std::size_t>(outputY) * absolutePitch;
            }
            else
            {
                destinationRow = destinationBytes + static_cast<std::size_t>(destinationHeight - 1 - outputY) * absolutePitch;
            }

            std::memset(destinationRow, 0, absolutePitch);
        }

        for (std::uint32_t outputY = 0; outputY < scaledHeight; ++outputY)
        {
            const std::uint32_t sourceY = static_cast<std::uint32_t>(static_cast<std::uint64_t>(outputY) * sourceHeight / scaledHeight);
            std::uint8_t* destinationRow = nullptr;

            if (destinationPitch >= 0)
            {
                destinationRow = destinationBytes + static_cast<std::size_t>(outputY) * absolutePitch;
            }
            else
            {
                destinationRow = destinationBytes + static_cast<std::size_t>(scaledHeight - 1 - outputY) * absolutePitch;
            }

            auto* destinationPixels = reinterpret_cast<std::uint32_t*>(destinationRow);
            const auto* sourceRow = s_binkDecodeBuffer.data() + static_cast<std::size_t>(sourceY) * sourceWidth;

            for (std::uint32_t outputX = 0; outputX < scaledWidth; ++outputX)
            {
                const std::int64_t physicalX = destinationLeft + static_cast<std::int64_t>(outputX);

                if (physicalX < 0 || physicalX >= static_cast<std::int64_t>(destinationWidth))
                {
                    continue;
                }

                const std::uint32_t sourceX = static_cast<std::uint32_t>(static_cast<std::uint64_t>(outputX) * sourceWidth / scaledWidth);
                destinationPixels[static_cast<std::size_t>(physicalX)] = sourceRow[sourceX];
            }
        }

        return result;
    }

	inline static BackyardBaseball2005Fix* s_instance_ = nullptr;
};

static std::unique_ptr<BackyardBaseball2005Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<BackyardBaseball2005Fix>(hModule);
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