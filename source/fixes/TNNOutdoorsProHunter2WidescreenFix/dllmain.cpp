#include "..\..\common\FixBase.hpp"
#include "..\..\common\DllNotificationWatcher.cpp"

class TNNOutdoorsProHunter2Fix final : public FixBase
{
public:
	explicit TNNOutdoorsProHunter2Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TNNOutdoorsProHunter2Fix() override
	{
		if (m_gameWatcher)
		{
			m_gameWatcher->Stop();
			m_gameWatcher.reset();
		}

		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TNNOutdoorsProHunter2WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.5";
	}

	const char* TargetName() const override
	{
		return "TNN Outdoors Pro Hunter 2";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "TNNPH2.EXE") ||
		Util::stringcmp_caseless(exeName, "CLIENT.EXE");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_zoomFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "CLIENT.EXE"))
		{
			m_clientShellDllModule = Memory::GetHandle("cshell.dll");
			m_clientShellDllModuleName = Memory::GetModuleName(m_clientShellDllModule);

			m_resolutionScansResult = Memory::PatternScan(ExeModule(), "8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50", "74 ?? 8B 46 ?? 8B 0D ?? ?? ?? ?? 89 7C 24", m_clientShellDllModule,
			"ff 91 ?? ?? ?? ?? 8b 15 ?? ?? ?? ?? ff 52");
			if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());
				spdlog::info("Centered Smacker Movies Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[CenteredSmackerMovies] - (std::uint8_t*)ExeModule());
				spdlog::info("Centered Splash Screen Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_resolutionScansResult[CenteredSplashScreen] - (std::uint8_t*)m_clientShellDllModule);

				m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
				{
					s_instance_->ResolutionMidHook(ctx);
				});

				Memory::PatchBytes(m_resolutionScansResult[CenteredSmackerMovies], "\xEB");

				m_splashScreenCenterHook = safetyhook::create_mid(m_resolutionScansResult[CenteredSplashScreen], [](SafetyHookContext& ctx)
				{
					auto* stack = reinterpret_cast<std::uintptr_t*>(ctx.esp);
					auto* destinationRect = reinterpret_cast<Rect*>(stack[2]);
					const auto* sourceRect = reinterpret_cast<const Rect*>(stack[3]);

					if (destinationRect == nullptr || sourceRect == nullptr)
					{
						return;
					}

					const std::int32_t destinationWidth = destinationRect->right - destinationRect->left;
					const std::int32_t destinationHeight = destinationRect->bottom - destinationRect->top;
					const std::int32_t sourceWidth = sourceRect->right - sourceRect->left;
					const std::int32_t sourceHeight = sourceRect->bottom - sourceRect->top;

					if (destinationWidth <= 0 || destinationHeight <= 0 || sourceWidth <= 0 || sourceHeight <= 0)
					{
						return;
					}

					const double scaleX = static_cast<double>(destinationWidth) / static_cast<double>(sourceWidth);
					const double scaleY = static_cast<double>(destinationHeight) / static_cast<double>(sourceHeight);
					const double scale = std::min(scaleX, scaleY);

					const auto scaledWidth = static_cast<std::int32_t>(std::lround(static_cast<double>(sourceWidth) * scale));
					const auto scaledHeight = static_cast<std::int32_t>(std::lround(static_cast<double>(sourceHeight) * scale));

					const std::int32_t x = destinationRect->left + ((destinationWidth - scaledWidth) / 2);
					const std::int32_t y = destinationRect->top + ((destinationHeight - scaledHeight) / 2);

					destinationRect->left = x;
					destinationRect->top = y;
					destinationRect->right = x + scaledWidth;
					destinationRect->bottom = y + scaledHeight;
				});
			}

			m_cameraFOVScansResult = Memory::PatternScan(m_clientShellDllModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 51 FF 92",
			"C7 85 ?? ?? ?? ?? ?? ?? ?? ?? C7 85", "8B 44 24 ?? 50 A1", "C7 44 24 ?? ?? ?? ?? ?? FF 91 ?? ?? ?? ?? 83 C4 ?? 8B F8",
			"D8 1D ?? ?? ?? ?? DF E0 F6 C4 ?? 75 ?? D9 44 24 ?? D8 64 24");
			if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
			{
				spdlog::info("Unzoomed Camera FOV Instructions 1 Scan: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Unzoomed1] - (std::uint8_t*)m_clientShellDllModule);
				spdlog::info("Unzoomed Camera FOV Instructions 2 Scan: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Unzoomed2] - (std::uint8_t*)m_clientShellDllModule);
				spdlog::info("Runtime VFOV Instructions Scan: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[RuntimeVFOV] - (std::uint8_t*)m_clientShellDllModule);
				spdlog::info("Zoomed Camera HFOV Instruction 1: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Zoomed1] - (std::uint8_t*)m_clientShellDllModule);
				spdlog::info("Zoomed Camera HFOV Instruction 2: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_cameraFOVScansResult[Zoomed2] - (std::uint8_t*)m_clientShellDllModule);

				Memory::WriteNOPs(m_cameraFOVScansResult[Unzoomed2], 20);

				m_unzoomedFOV2Hook = safetyhook::create_mid(m_cameraFOVScansResult[Unzoomed2], [](SafetyHookContext& ctx)
				{
					s_instance_->m_newUnzoomedHFOV2 = Maths::CalculateNewHFOV_RadBased(m_defaultHFOV, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
					s_instance_->m_newUnzoomedVFOV2 = Maths::CalculateNewVFOV_RadBased(m_defaultVFOV, s_instance_->m_fovFactor);
					*reinterpret_cast<float*>(ctx.ebp + 0xFF08) = s_instance_->m_newUnzoomedHFOV2;
					*reinterpret_cast<float*>(ctx.ebp + 0xFF0C) = s_instance_->m_newUnzoomedVFOV2;
				});

				Memory::WriteNOPs(m_cameraFOVScansResult[RuntimeVFOV], 4);

				m_runtimeVFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[RuntimeVFOV], [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentHFOV = *reinterpret_cast<float*>(ctx.esp + 0x08);
					s_instance_->m_currentVFOV = Maths::CalculateNewHFOV_RadBased(s_instance_->m_currentHFOV, 1.0f / s_instance_->m_newAspectRatio);
					ctx.eax = std::bit_cast<std::uintptr_t>(s_instance_->m_currentVFOV);
				});

				Memory::WriteNOPs(m_cameraFOVScansResult[Zoomed1], 8);

				m_zoomedFOV1Hook = safetyhook::create_mid(m_cameraFOVScansResult[Zoomed1], [](SafetyHookContext& ctx)
				{
					s_instance_->m_newZoomedHFOV = Maths::CalculateNewHFOV_RadBased(m_originalZoomedHFOV, s_instance_->m_aspectRatioScale) / s_instance_->m_zoomFactor;
					*reinterpret_cast<float*>(ctx.esp + 0xC) = s_instance_->m_newZoomedHFOV;
				});

				m_zoomedHFOVComparisonsAddress = Memory::GetPointerFromAddress(m_cameraFOVScansResult[Zoomed2] + 2, Memory::PointerMode::Absolute);
			}

			if (m_skipIntroVideos == true)
			{
				m_skipIntroVideosScansResult = Memory::PatternScan(m_clientShellDllModule, "6A ?? 68 ?? ?? ?? ?? 56 8B CF E8 ?? ?? ?? ?? 84 C0 75 ?? 6A ?? 56 8B CF E8 ?? ?? ?? ?? 5F 5E C3 C7 87 ?? ?? ?? ?? ?? ?? ?? ?? 5F 5E C3 8B 06",
				"75 ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CE");
				if (Memory::AreAllSignaturesValid(m_skipIntroVideosScansResult) == true)
				{
					spdlog::info("Intro Videos Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_skipIntroVideosScansResult[IntroVideos] - (std::uint8_t*)m_clientShellDllModule);
					spdlog::info("Splash Screen Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_skipIntroVideosScansResult[SplashScreen] - (std::uint8_t*)m_clientShellDllModule);

					Memory::PatchBytes(m_skipIntroVideosScansResult[IntroVideos], "\xEB\x11");
					Memory::PatchBytes(m_skipIntroVideosScansResult[SplashScreen], "\xEB");
				}
			}
		}

		m_gameWatcher = std::make_unique<DllNotificationWatcher>(
		// OnLoad
		[this](HMODULE module)
		{
			const std::string moduleName = Memory::GetModuleName(module);				

			if (Util::stringcmp_caseless(moduleName, "D3D.REN"))
			{
				spdlog::info("{:s} loaded.", moduleName.c_str());

				m_bitDepthCheckScanResult = Memory::PatternScan(module, "0F 85 ?? ?? ?? ?? C7 04 24");
				if (m_bitDepthCheckScanResult)
				{
					spdlog::info("Bit Depth Check Instruction: Address is {:s}+{:x}", moduleName.c_str(), m_bitDepthCheckScanResult - (std::uint8_t*)module);

					Memory::WriteNOPs(m_bitDepthCheckScanResult, 6);
				}
				else
				{
					spdlog::error("Failed to locate bit depth check instruction memory address.");
					return;
				}
			}
		},
		// OnUnload
		[this](HMODULE module)
		{
			const std::string moduleName = Memory::GetModuleName(module);

			if (Util::stringcmp_caseless(moduleName, "D3D.REN"))
			{
				spdlog::info("{:s} unloaded.", moduleName.c_str());
			}
		});

		if (!m_gameWatcher->Start())
		{
			spdlog::error("DllNotificationWatcher failed. NTSTATUS=0x{:08X}, Win32Error={}.", static_cast<unsigned long>(m_gameWatcher->LastNtStatus()), m_gameWatcher->LastWin32Error());
			m_gameWatcher.reset();
			return;
		}
	}

private:
	HMODULE m_clientShellDllModule = nullptr;
	std::string m_clientShellDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_defaultHFOV = 1.5707963705062866f; // 90 degrees in radians
	static constexpr float m_defaultVFOV = 1.3089969158172607f; // 75 degrees in radians
	static constexpr float m_originalZoomedHFOV = 0.174532935f; // 10 degrees in radians

	bool m_skipIntroVideos = false;

	float m_zoomFactor = 0.0f;

	float m_currentHFOV = 0.0f;
	float m_currentVFOV = 0.0f;
	float m_newUnzoomedHFOV1 = 0.0f;
	float m_newUnzoomedVFOV1 = 0.0f;
	float m_newUnzoomedHFOV2 = 0.0f;
	float m_newUnzoomedVFOV2 = 0.0f;
	float m_newZoomedHFOV = 0.0f;
	uintptr_t m_zoomedHFOVComparisonsAddress = 0;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::vector<std::uint8_t*> m_skipIntroVideosScansResult{};
	std::uint8_t* m_bitDepthCheckScanResult = nullptr;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_splashScreenCenterHook{};
	SafetyHookMid m_unzoomedFOV2Hook{};
	SafetyHookMid m_runtimeVFOVHook{};
	SafetyHookMid m_zoomedFOV1Hook{};

	struct Rect
	{
		std::int32_t left;
		std::int32_t top;
		std::int32_t right;
		std::int32_t bottom;
	};

	enum ResolutionInstructionsIndex
	{
		WidthHeight,
		CenteredSmackerMovies,
		CenteredSplashScreen
	};

	enum CameraFOVInstructionsIndex
	{
		Unzoomed1,
		Unzoomed2,
		RuntimeVFOV,
		Zoomed1,
		Zoomed2
	};

	enum SkipIntroVideosInstructionsIndex
	{
		IntroVideos,
		SplashScreen
	};

	void ResolutionMidHook(SafetyHookContext& ctx)
	{
		m_newResX = Memory::ReadMem(ctx.eax + 0x34);
		m_newResY = Memory::ReadMem(ctx.eax + 0x38);
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
		WriteStaticFOVs();
	}

	void WriteStaticFOVs()
	{
		m_newUnzoomedHFOV1 = Maths::CalculateNewHFOV_RadBased(m_defaultHFOV, m_aspectRatioScale, m_fovFactor);
		m_newUnzoomedVFOV1 = Maths::CalculateNewVFOV_RadBased(m_defaultVFOV, m_fovFactor);
		m_newZoomedHFOV = Maths::CalculateNewHFOV_RadBased(m_originalZoomedHFOV, m_aspectRatioScale) / m_zoomFactor;

		Memory::Write(m_cameraFOVScansResult[Unzoomed1] + 6, m_newUnzoomedHFOV1);
		Memory::Write(m_cameraFOVScansResult[Unzoomed1] + 1, m_newUnzoomedVFOV1);

		Memory::Write(m_zoomedHFOVComparisonsAddress, m_newZoomedHFOV);
	}

	std::unique_ptr<DllNotificationWatcher> m_gameWatcher;

	inline static TNNOutdoorsProHunter2Fix* s_instance_ = nullptr;
};

static std::unique_ptr<TNNOutdoorsProHunter2Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<TNNOutdoorsProHunter2Fix>(hModule);
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