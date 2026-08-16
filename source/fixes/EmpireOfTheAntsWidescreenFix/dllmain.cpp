#include "..\..\common\FixBase.hpp"

class EmpireOfTheAntsFix final : public FixBase
{
public:
	explicit EmpireOfTheAntsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~EmpireOfTheAntsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EmpireOfTheAntsWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Empire of the Ants";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "InitVid.exe") ||
		Util::stringcmp_caseless(exeName, "Game.exe");
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
		if (Util::stringcmp_caseless(ExeName(), "InitVid.exe"))
		{
			m_resolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "0F 8C ?? ?? ?? ?? DB 44 24");
			if (m_resolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_resolutionListUnlockScanResult, 6);
				Memory::WriteNOPs(m_resolutionListUnlockScanResult + 38, 2);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Game.exe"))
		{
			m_resolutionScansResult = Memory::PatternScan(ExeModule(), "7C ?? DB 44 24 ?? DC 0D", "8B 4C 24 ?? 8B 44 24 ?? 89 0D");
			if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_resolutionScansResult[ListUnlock], 2);
				Memory::WriteNOPs(m_resolutionScansResult[ListUnlock] + 26, 2);

				m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
				{
					uint32_t& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x4);
					uint32_t& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x8);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
					s_instance_->m_resolutionHook.disable();
				});
			}

			m_movieRectangleScanResult = Memory::PatternScan(ExeModule(), "8B 10 51 50 C7 44 24");
			if (m_movieRectangleScanResult)
			{
				spdlog::info("Bink Movie Rectangle Hook: Address is {:s}+{:x}", ExeName().c_str(), m_movieRectangleScanResult - reinterpret_cast<std::uint8_t*>(ExeModule()));

				m_movieRectangleHook = safetyhook::create_mid(m_movieRectangleScanResult, [](SafetyHookContext& ctx)
				{
					const uintptr_t gameBase = reinterpret_cast<std::uintptr_t>(s_instance_->ExeModule());
					constexpr std::uintptr_t BinkCallerReturnRva = 0x8C887;
					const uintptr_t callerReturnAddress = Memory::ReadMem(ctx.esp + 0xA0);

					if (callerReturnAddress != gameBase + BinkCallerReturnRva)
					{
						return;
					}

					constexpr std::uintptr_t BinkHandleRva = 0x16D048;
					const std::uint32_t binkAddress = Memory::ReadMem(gameBase + BinkHandleRva);
					auto* const bink = reinterpret_cast<BinkHeader*>(static_cast<std::uintptr_t>(binkAddress));

					if (bink == nullptr)
					{
						return;
					}

					const std::uint32_t videoWidth = bink->width;
					const std::uint32_t videoHeight = bink->height;

					if (!IsReasonableDimension(videoWidth) || !IsReasonableDimension(videoHeight))
					{
						return;
					}

					constexpr std::uintptr_t ScreenWidthRva = 0x17BEE0;
					constexpr std::uintptr_t ScreenHeightRva = 0x17BEE4;

					const std::uint32_t screenWidth = Memory::ReadMem(gameBase + ScreenWidthRva);
					const std::uint32_t screenHeight = Memory::ReadMem(gameBase + ScreenHeightRva);

					if (!IsReasonableDimension(screenWidth) || !IsReasonableDimension(screenHeight))
					{
						return;
					}

					const std::uint64_t widthConstrainedProduct = static_cast<std::uint64_t>(screenWidth) * videoHeight;
					const std::uint64_t heightConstrainedProduct = static_cast<std::uint64_t>(screenHeight) * videoWidth;

					std::uint32_t scaledWidth = 0;
					std::uint32_t scaledHeight = 0;

					if (widthConstrainedProduct <= heightConstrainedProduct)
					{
						scaledWidth = screenWidth;
						scaledHeight = static_cast<std::uint32_t>(static_cast<std::uint64_t>(videoHeight) * screenWidth / videoWidth);
					}
					else
					{
						scaledHeight = screenHeight;
						scaledWidth = static_cast<std::uint32_t>(static_cast<std::uint64_t>(videoWidth) * screenHeight / videoHeight);
					}

					if (scaledWidth == 0 || scaledHeight == 0)
					{
						return;
					}

					scaledWidth = std::min(scaledWidth, screenWidth);
					scaledHeight = std::min(scaledHeight, screenHeight);

					const std::int32_t left = static_cast<std::int32_t>((screenWidth - scaledWidth) / 2);
					const std::int32_t top = static_cast<std::int32_t>((screenHeight - scaledHeight) / 2);

					auto* const destination = reinterpret_cast<RECT*>(ctx.esp + 0x2C);
					destination->left = left;
					destination->top = top;
					destination->right = left + static_cast<std::int32_t>(scaledWidth);
					destination->bottom = top + static_cast<std::int32_t>(scaledHeight);
				});
			}
			else
			{
				spdlog::error("Failed to locate the Bink movie destination rectangle.");
			}

			m_x3dDllModule = Memory::GetHandle("x3d.dll");
			m_x3dDllModuleName = Memory::GetModuleName(m_x3dDllModule);

			m_cameraFOVScansResult = Memory::PatternScan(m_x3dDllModule, "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 49",
			"DC 3D ?? ?? ?? ?? D9 C0 D8 48", "DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? 8D 44 24",
			"DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8B 46",
			"DC 3D ?? ?? ?? ?? D9 5C 24 ?? D9 44 24 ?? D8 48 ?? D9 40 ?? D8 4E ?? DE F9 D9 5C 24 ?? FF 15 ?? ?? ?? ?? 8D 44 24",
			"D9 46 ?? D8 0D ?? ?? ?? ?? D8 0D", "D9 44 24 ?? D9 F2");
			if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), m_cameraFOVScansResult[FOV1] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), m_cameraFOVScansResult[FOV2] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), m_cameraFOVScansResult[FOV3] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), m_cameraFOVScansResult[FOV4] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), m_cameraFOVScansResult[FOV5] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), m_cameraFOVScansResult[FOV6] - (std::uint8_t*)m_x3dDllModule);
				spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", m_x3dDllModuleName.c_str(), m_cameraFOVScansResult[FOV7] - (std::uint8_t*)m_x3dDllModule);

				Memory::WriteNOPs(m_cameraFOVScansResult[FOV1], 6);

				m_cameraFOV1Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV1], CameraFOVMidHook);

				Memory::WriteNOPs(m_cameraFOVScansResult[FOV2], 6);

				m_cameraFOV2Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV2], CameraFOVMidHook);

				Memory::WriteNOPs(m_cameraFOVScansResult[FOV3], 6);

				m_cameraFOV3Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV3], CameraFOVMidHook);

				Memory::WriteNOPs(m_cameraFOVScansResult[FOV4], 6);

				m_cameraFOV4Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV4], CameraFOVMidHook);

				Memory::WriteNOPs(m_cameraFOVScansResult[FOV5], 6);

				m_cameraFOV5Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV5], CameraFOVMidHook);

				Memory::WriteNOPs(m_cameraFOVScansResult[FOV6], 3);

				m_cameraFOV6Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV6], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV6 = Memory::ReadMem(ctx.esi + 0x50);
					s_instance_->m_newCameraFOV6 = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV6, 1.0f / s_instance_->m_aspectRatioScale) / (float)s_instance_->m_fovFactor;
					FPU::FLD(s_instance_->m_newCameraFOV6);
				});

				Memory::WriteNOPs(m_cameraFOVScansResult[FOV7], 4);

				m_newCameraFOV7 = 5.0f;

				m_cameraFOV7Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV7], [](SafetyHookContext& ctx)
				{
					FPU::FLD(s_instance_->m_newCameraFOV7);
				});
			}

			if (m_skipIntroVideos == true)
			{
				m_skipIntroVideosScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 90");
				if (m_skipIntroVideosScanResult)
				{
					spdlog::info("Skip Intro Videos Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScanResult - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(m_skipIntroVideosScanResult + 10, "\xE9\x8A\x00\x00\x00\x90");
				}
				else
				{
					spdlog::error("Failed to locate skip intro videos instruction memory address.");
					return;
				}
			}			
		}
	}

private:
	HMODULE m_x3dDllModule = nullptr;
	std::string m_x3dDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr int m_newMovieState = 0;

	bool m_skipIntroVideos = false;

	std::uint8_t* m_resolutionListUnlockScanResult = nullptr;
	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::uint8_t* m_movieRectangleScanResult = nullptr;
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::uint8_t* m_skipIntroVideosScanResult = nullptr;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_movieRectangleHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};
	SafetyHookMid m_cameraFOV5Hook{};
	SafetyHookMid m_cameraFOV6Hook{};
	SafetyHookMid m_cameraFOV7Hook{};

	static void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		s_instance_->m_newCameraFOV = (1.0 / (double)s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor;
		FPU::FDIVR(s_instance_->m_newCameraFOV);
	}

	double m_newCameraFOV = 0.0;
	float m_newCameraFOV6 = 0.0f;
	float m_newCameraFOV7 = 0.0f;

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
	};

	enum CameraHFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7
	};

	struct BinkHeader
	{
		std::uint32_t width;
		std::uint32_t height;
		std::uint32_t frameCount;
		std::uint32_t currentFrame;
	};

	static bool IsReasonableDimension(const std::uint32_t value)
	{
		return value > 0 && value <= 16384;
	}

	inline static EmpireOfTheAntsFix* s_instance_ = nullptr;
};

static std::unique_ptr<EmpireOfTheAntsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<EmpireOfTheAntsFix>(hModule);
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