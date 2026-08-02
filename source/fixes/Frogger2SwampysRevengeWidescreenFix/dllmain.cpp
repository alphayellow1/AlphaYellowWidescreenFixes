#include "..\..\common\FixBase.hpp"

class Frogger2Fix final : public FixBase
{
public:
	explicit Frogger2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~Frogger2Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Frogger2SwampysRevengeWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Frogger 2: Swampy's Revenge";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Frogger2(1).exe");
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
		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "BB ?? ?? ?? ?? 8B 16", "8B 8C 24 ?? ?? ?? ?? 8B 94 24",
		"A1 ?? ?? ?? ?? 33 F6 56", "A1 ?? ?? ?? ?? 81 EC ?? ?? ?? ?? 53");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());
			spdlog::info("Bink Video Rectangle Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkVideoRect] - (std::uint8_t*)ExeModule());
			spdlog::info("Bink Video Handle Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkVideoHandle] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(m_resolutionScansResult[ListUnlock] + 1, "\x01");
			Memory::PatchBytes(m_resolutionScansResult[ListUnlock] + 57, "\x00");
			
			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x88);
				s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x8C);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});

			m_binkVideoRectHook = safetyhook::create_mid(m_resolutionScansResult[BinkVideoRect], [](SafetyHookContext & ctx)
			{
				const uintptr_t binkHandleAddress = Memory::GetPointerFromAddress(s_instance_->m_resolutionScansResult[BinkVideoHandle] + 1, Memory::PointerMode::Absolute);
				const uintptr_t binkHandle = Memory::ReadMem(binkHandleAddress);

				if (binkHandle == 0 || s_instance_->m_newResX <= 0 || s_instance_->m_newResY <= 0)
				{
					return;
				}

				const std::int32_t screenWidth = s_instance_->m_newResX;
				const std::int32_t screenHeight = s_instance_->m_newResY;

				const std::uint32_t videoWidth = Memory::ReadMem(binkHandle);
				const std::uint32_t videoHeight = Memory::ReadMem(binkHandle + 0x4);

				std::int32_t drawWidth = 0;
				std::int32_t drawHeight = 0;

				if (static_cast<std::int64_t>(screenWidth) * videoHeight <=static_cast<std::int64_t>(screenHeight) * videoWidth)
				{
					drawWidth = screenWidth;
					drawHeight = static_cast<std::int32_t>(static_cast<std::int64_t>(drawWidth) * videoHeight / videoWidth);
				}
				else
				{
					drawHeight = screenHeight;
					drawWidth = static_cast<std::int32_t>(static_cast<std::int64_t>(drawHeight) * videoWidth / videoHeight);
				}

				RECT& destinationRect = Memory::ReadMem(ctx.esp + 0x10);
				destinationRect.left = (screenWidth - drawWidth) / 2;
				destinationRect.top = (screenHeight - drawHeight) / 2;
				destinationRect.right = destinationRect.left + drawWidth;
				destinationRect.bottom = destinationRect.top + drawHeight;
			});
		}

		m_cameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? D9 1D");
		if (m_cameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_cameraFOVAddress = Memory::GetPointerFromAddress(m_cameraFOVScanResult + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(m_cameraFOVScanResult, 10);

			m_cameraFOVHook = safetyhook::create_mid(m_cameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraFOV = (uint32_t)((m_originalCameraFOV / s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor);

				Memory::Write(s_instance_->m_cameraFOVAddress, s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "6A ?? E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 C4 ?? 85 C0 75 ?? 6A ?? E8 ?? ?? ?? ?? A1");
			if (m_skipIntroVideosScanResult)
			{
				spdlog::info("Skip Intro Videos Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_skipIntroVideosScanResult, "\xEB\x2E");
			}
			else
			{
				spdlog::error("Failed to locate skip intro videos instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr uint32_t m_originalCameraFOV = 450;

	bool m_skipIntroVideos = true;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::uint8_t* m_cameraFOVScanResult{};
	std::uint8_t* m_skipIntroVideosScanResult{};

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};
	SafetyHookMid m_binkVideoRectHook{};

	uintptr_t m_cameraFOVAddress = 0;
	uint32_t m_newCameraFOV = 0;

	enum ResolutionListsIndices
	{
		ListUnlock,
		WidthHeight,
		BinkVideoRect,
		BinkVideoHandle
	};

	inline static Frogger2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<Frogger2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<Frogger2Fix>(hModule);
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