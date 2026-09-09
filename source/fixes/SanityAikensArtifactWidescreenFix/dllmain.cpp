#include "..\..\common\FixBase.hpp"
#include "..\..\common\DllNotificationWatcher.cpp"
#include <ddraw.h>

class SanityAikensArtifactFix final : public FixBase
{
public:
	explicit SanityAikensArtifactFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SanityAikensArtifactFix() override
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
		return "SanityAikensArtifactWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.8";
	}

	const char* TargetName() const override
	{
		return "Sanity: Aiken's Artifact";
	}

	InitMode GetInitMode() const override
	{
		if (Util::stringcmp_caseless(ExeName(), "Sanity.exe"))
		{
			return InitMode::Direct;
		}

		if (Util::stringcmp_caseless(ExeName(), "client.dll"))
		{
			return InitMode::WorkerThread;
		}
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Sanity.exe") ||
		Util::stringcmp_caseless(exeName, "client.dll");
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
		if (Util::stringcmp_caseless(ExeName(), "client.dll"))
		{
			m_resolutionScansResult = Memory::PatternScan(ExeModule(), "8B 48 ?? 89 0D ?? ?? ?? ?? 8B 50", "8B 55 ?? 51 55");
			if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());
				spdlog::info("Bink Video Rect Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[BinkVideoRect] - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
				{
					s_instance_->ResolutionMidHook(ctx);
				});

				m_binkVideoRectHook = safetyhook::create_mid(m_resolutionScansResult[BinkVideoRect], [](SafetyHookContext & ctx)
				{
					const auto bink = *reinterpret_cast<std::uintptr_t*>(ctx.esi + 0x18);

					if (!bink)
					{
						return;
					}

					const auto videoWidth = *reinterpret_cast<std::uint32_t*>(bink);
					const auto videoHeight = *reinterpret_cast<std::uint32_t*>(bink + 0x4);

					if (!videoWidth || !videoHeight)
					{
						return;
					}

					auto* const destinationRect = reinterpret_cast<RECT*>(ctx.esp + 0x30);
					const std::int32_t screenWidth = destinationRect->right - destinationRect->left;
					const std::int32_t screenHeight = destinationRect->bottom - destinationRect->top;

					if (screenWidth <= 0 || screenHeight <= 0)
					{
						return;
					}

					const auto scaledWidth = static_cast<std::int32_t>((static_cast<std::uint64_t>(screenHeight) * videoWidth) / videoHeight);
					const auto left = (screenWidth - scaledWidth) / 2;

					auto* const backBuffer = reinterpret_cast<IDirectDrawSurface*>(ctx.ebp);

					if (backBuffer)
					{
						DDBLTFX bltFx{};
						bltFx.dwSize = sizeof(bltFx);
						bltFx.dwFillColor = 0;
						backBuffer->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &bltFx);
					}

					destinationRect->left = left;
					destinationRect->top = 0;
					destinationRect->right = left + scaledWidth;
					destinationRect->bottom = screenHeight;
				});
			}

			m_cameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B B0 ?? ?? ?? ?? 89 B4 24 ?? ?? ?? ?? 8B B0 ?? ?? ?? ?? 89 B4 24");
			if (m_cameraFOVScanResult)
			{
				spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScanResult - (std::uint8_t*)ExeModule());
				spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScanResult + 13 - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(m_cameraFOVScanResult, 6);

				m_cameraHFOVHook = safetyhook::create_mid(m_cameraFOVScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentCameraHFOV = Memory::ReadMem(ctx.eax + 0x154);
					s_instance_->m_currentCameraVFOV = Memory::ReadMem(ctx.eax + 0x158);

					if (s_instance_->m_currentCameraHFOV == s_instance_->m_defaultCameraHFOV && Maths::isClose(s_instance_->m_currentCameraVFOV, s_instance_->m_defaultCameraHFOV))
					{
						s_instance_->m_newCameraHFOV = Maths::CalculateNewHFOV_RadBased(s_instance_->m_currentCameraHFOV, s_instance_->m_aspectRatioScale); // Main/Pause Menu HFOV
					}
					else if (s_instance_->m_currentCameraHFOV == s_instance_->m_defaultCameraHFOV && Maths::isClose(s_instance_->m_currentCameraVFOV, s_instance_->m_defaultCameraVFOV / s_instance_->m_aspectRatioScale))
					{
						s_instance_->m_newCameraHFOV = Maths::CalculateNewHFOV_RadBased(s_instance_->m_currentCameraHFOV, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor); // Default HFOV
					}
					else
					{
						s_instance_->m_newCameraHFOV = Maths::CalculateNewHFOV_RadBased(s_instance_->m_currentCameraHFOV, s_instance_->m_aspectRatioScale); // Gameplay HFOVs
					}

					ctx.esi = std::bit_cast<uintptr_t>(s_instance_->m_newCameraHFOV);
				});

				Memory::WriteNOPs(m_cameraFOVScanResult + 13, 6);

				m_cameraVFOVHook = safetyhook::create_mid(m_cameraFOVScanResult + 13, [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentCameraHFOV = Memory::ReadMem(ctx.eax + 0x154);
					s_instance_->m_currentCameraVFOV = Memory::ReadMem(ctx.eax + 0x158);

					if (s_instance_->m_currentCameraHFOV == s_instance_->m_defaultCameraHFOV && Maths::isClose(s_instance_->m_currentCameraVFOV, s_instance_->m_defaultCameraHFOV))
					{
						s_instance_->m_newCameraVFOV = s_instance_->m_currentCameraVFOV; // Main/Pause Menu VFOV
					}
					else if (s_instance_->m_currentCameraHFOV == s_instance_->m_defaultCameraHFOV && Maths::isClose(s_instance_->m_currentCameraVFOV, s_instance_->m_defaultCameraVFOV / s_instance_->m_aspectRatioScale))
					{
						s_instance_->m_newCameraVFOV = Maths::CalculateNewVFOV_RadBased(s_instance_->m_currentCameraVFOV * s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor); // Default VFOV
					}
					else
					{
						s_instance_->m_newCameraVFOV = Maths::CalculateNewVFOV_RadBased(s_instance_->m_currentCameraVFOV * s_instance_->m_aspectRatioScale); // Gameplay VFOVs
					}

					ctx.esi = std::bit_cast<uintptr_t>(s_instance_->m_newCameraVFOV);
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instructions scan memory address.");
				return;
			}

			if (m_skipIntroVideos == true)
			{
				m_clientShellDllModule = Memory::GetHandle("cshell.dll");
				m_clientShellDllModuleName = Memory::GetModuleName(m_clientShellDllModule);

				m_skipIntroVideosScanResult = Memory::PatternScan(m_clientShellDllModule, "A1 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 90 ?? ?? ?? ?? 83 C4 ?? 85 C0 0F 84");
				if (m_skipIntroVideosScanResult)
				{
					spdlog::info("Intro Videos Instruction: Address is {:s}+{:x}", m_clientShellDllModuleName.c_str(), m_skipIntroVideosScanResult - (std::uint8_t*)m_clientShellDllModule);

					m_skipIntroVideosHook = safetyhook::create_mid(m_skipIntroVideosScanResult, [](SafetyHookContext & ctx)
					{
						ctx.eip = reinterpret_cast<std::uintptr_t>(s_instance_->m_clientShellDllModule) + 0x2B84C;
					});
				}
				else
				{
					spdlog::error("Failed to locate skip intro videos instruction memory address.");
					return;
				}
			}			
		}

		if (Util::stringcmp_caseless(ExeName(), "Sanity.exe"))
		{
			if (m_runMultipleInstances == true)
			{
				m_multipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "74 ?? 89 35 ?? ?? ?? ?? 8D 94 24");
				if (m_multipleInstancesCheckScanResult)
				{
					spdlog::info("Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_multipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

					Memory::WriteNOPs(m_multipleInstancesCheckScanResult, 2);
				}
				else
				{
					spdlog::error("Failed to locate multiple instances check instruction memory address.");
					return;
				}
			}
		}

		m_gameWatcher = std::make_unique<DllNotificationWatcher>(
			// OnLoad
			[this](HMODULE module)
			{
				const std::string moduleName = Memory::GetModuleName(module);

				if (Util::stringcmp_caseless(moduleName, "d3d.ren"))
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

				if (Util::stringcmp_caseless(moduleName, "d3d.ren"))
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
	static constexpr float m_defaultCameraHFOV = 1.5707963705062866f; // 90 degrees in radians
	static constexpr float m_defaultCameraVFOV = 1.1780972480773926f; // 67.5 degrees in radians

	float m_currentCameraHFOV = 0.0f;
	float m_currentCameraVFOV = 0.0f;
	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::uint8_t* m_skipIntroVideosScanResult = nullptr;
	std::uint8_t* m_bitDepthCheckScanResult = nullptr;
	std::uint8_t* m_multipleInstancesCheckScanResult = nullptr;
	std::uint8_t* m_cameraFOVScanResult = nullptr;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_binkVideoRectHook{};
	SafetyHookMid m_skipIntroVideosHook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	enum ResolutionInstructionsIndex
	{
		WidthHeight,
		BinkVideoRect
	};

	enum CameraFOVInstructionsIndex
	{
		HFOV,
		VFOV
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
	}

	std::unique_ptr<DllNotificationWatcher> m_gameWatcher;

	inline static SanityAikensArtifactFix* s_instance_ = nullptr;
};

static std::unique_ptr<SanityAikensArtifactFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<SanityAikensArtifactFix>(hModule);
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