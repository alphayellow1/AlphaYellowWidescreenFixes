#include "..\..\common\FixBase.hpp"

class StarWarsStarfighterFix final : public FixBase
{
public:
	explicit StarWarsStarfighterFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~StarWarsStarfighterFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "StarWarsStarfighterWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Star Wars: Starfighter";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Starfighter.exe") ||
		Util::stringcmp_caseless(exeName, "Europa.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "UnzoomedFOVFactor", m_unzoomedFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomedFOVFactor", m_zoomedFOVFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_unzoomedFOVFactor);
		spdlog_confparse(m_zoomedFOVFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "Starfighter.exe"))
		{
			m_resolutionListUnlockScansResult = Memory::PatternScan(ExeModule(), "72 ?? 8B 46 ?? 3D",
			"81 F9 ?? ?? ?? ?? 75 ?? 8B 46 ?? 3D ?? ?? ?? ?? 74 ?? 81 F9 ?? ?? ?? ?? 75 ?? 8B 46 ?? 3D ?? ?? ?? ?? 74 ?? 81 F9 ?? ?? ?? ?? 75 ?? 8B 46 ?? 3D ?? ?? ?? ?? 74",
			"81 FA ?? ?? ?? ?? 75 ?? 8B 41 ?? 3D ?? ?? ?? ?? 74 ?? 81 FA ?? ?? ?? ?? 75 ?? 8B 41 ?? 3D ?? ?? ?? ?? 74 ?? 81 FA ?? ?? ?? ?? 75 ?? 8B 41 ?? 3D ?? ?? ?? ?? 74");
			if (Memory::AreAllSignaturesValid(m_resolutionListUnlockScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListUnlockScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListUnlockScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution List Unlock Scan 3: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListUnlockScansResult[ListUnlock3] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_resolutionListUnlockScansResult[ListUnlock1], "\xEB");
				Memory::PatchBytes(m_resolutionListUnlockScansResult[ListUnlock2], "\x8B\x46\x08\xEB\x43\x90");
				Memory::PatchBytes(m_resolutionListUnlockScansResult[ListUnlock3], "\x8B\x41\x08\xEB\x43\x90");
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Europa.exe"))
		{
			GetDLLModulesInfo();

			m_resolutionScanResult = Memory::PatternScan(ExeModule(), "8A 45 ?? 88 86 ?? ?? ?? ?? 8A 45");
			if (m_resolutionScanResult)
			{
				spdlog::info("Resolution Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScanResult - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(m_resolutionScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->m_newResX = Memory::ReadMem(ctx.esi + 0x70);
					s_instance_->m_newResY = Memory::ReadMem(ctx.esi + 0x74);
					s_instance_->CalculateARAndScale();
				});
			}
			else
			{
				spdlog::info("Failed to locate resolution globals instruction memory address.");
				return;
			}

			m_uiModeResolutionScanResult = Memory::PatternScan(m_LECDirect3DDllModule, "C7 45 ?? ?? ?? ?? ?? C7 45");
			if (m_uiModeResolutionScanResult)
			{
				spdlog::info("UI Mode Resolution Instructions: Address is {:s}+{:x}", m_LECDirect3DDllModuleName, m_uiModeResolutionScanResult - reinterpret_cast<std::uint8_t*>(m_LECDirect3DDllModule));

				Memory::WriteNOPs(m_uiModeResolutionScanResult, 14);
			}
			else
			{
				spdlog::info("Failed to locate the forced 640x480 UI-mode resolution.");
				return;
			}

			m_uiModeViewportScanResult = Memory::PatternScan(m_LECDirect3DDllModule, "C6 83 ?? ?? ?? ?? ?? 81 C3");
			if (m_uiModeViewportScanResult)
			{
				spdlog::info("UI Mode Viewport Hook: Address is {:s}+{:x}", m_LECDirect3DDllModuleName, m_uiModeViewportScanResult - reinterpret_cast<std::uint8_t*>(m_LECDirect3DDllModule));

				m_uiModeViewportHook = safetyhook::create_mid(m_uiModeViewportScanResult, [](SafetyHookContext& ctx)
				{
					if (s_instance_->m_newResX <= 0 || s_instance_->m_newResY <= 0)
					{
						return;
					}

					const std::uintptr_t device = *reinterpret_cast<const std::uintptr_t*>(ctx.ebx + 0x134);

					if (device == 0)
					{
						return;
					}

					const std::uint32_t outputWidth = static_cast<std::uint32_t>(s_instance_->m_newResX);
					const std::uint32_t outputHeight = static_cast<std::uint32_t>(s_instance_->m_newResY);

					// Fit a centered 4:3 viewport inside the output
					std::uint32_t viewportWidth = static_cast<std::uint32_t>(outputHeight * 4.0f / 3.0f + 0.5f);
					std::uint32_t viewportHeight = outputHeight;

					if (viewportWidth > outputWidth)
					{
						viewportWidth = outputWidth;
						viewportHeight = static_cast<std::uint32_t>(outputWidth * 3.0f / 4.0f + 0.5f);
					}

					D3DViewport8 viewport{};

					viewport.X = (outputWidth - viewportWidth) / 2;
					viewport.Y = (outputHeight - viewportHeight) / 2;
					viewport.Width = viewportWidth;
					viewport.Height = viewportHeight;

					viewport.MinZ = 0.0f;
					viewport.MaxZ = 1.0f;

					const HRESULT32 result = SetD3D8Viewport(device, viewport);

					if (result < 0)
					{
						return;
					}
				});
			}
			else
			{
				spdlog::info("Failed to locate the UI mode viewport instruction address.");
				return;
			}

			m_binkCopyScanResult = Memory::PatternScan(m_LECDirect3DDllModule, "FF 15 ?? ?? ?? ?? 8B F8 A1");
			if (m_binkCopyScanResult)
			{
				spdlog::info("Bink Copy Instruction: Address is {:s}+{:x}", m_LECDirect3DDllModuleName, m_binkCopyScanResult - reinterpret_cast<std::uint8_t*>(m_LECDirect3DDllModule));

				m_binkCopyHook = safetyhook::create_mid(m_binkCopyScanResult, [](SafetyHookContext& ctx)
				{
					const std::uintptr_t binkHandle = *reinterpret_cast<const std::uintptr_t*>(ctx.esp);
					const std::uintptr_t backBufferBits = *reinterpret_cast<const std::uintptr_t*>(ctx.esp + 0x4);
					const std::uint32_t backBufferPitch = *reinterpret_cast<const std::uint32_t*>(ctx.esp + 0x8);

					if (binkHandle == 0 || backBufferBits == 0 || backBufferPitch == 0)
					{
						return;
					}

					const std::uint32_t movieWidth = *reinterpret_cast<const std::uint32_t*>(binkHandle);
					const std::uint32_t movieHeight = *reinterpret_cast<const std::uint32_t*>(binkHandle + 4);

					if (movieWidth == 0 || movieHeight == 0 || movieWidth > 8192 || movieHeight > 8192)
					{
						return;
					}

					const std::uint32_t outputWidth = s_instance_->m_newResX;
					const std::uint32_t outputHeight = s_instance_->m_newResY;

					if (outputWidth == 0 || outputHeight == 0)
					{
						return;
					}

					std::uint32_t bytesPerPixel = 0;

					if (backBufferPitch >= outputWidth * 4 && backBufferPitch < outputWidth * 4 + 256)
					{
						bytesPerPixel = 4;
					}
					else if (backBufferPitch >= outputWidth * 2 && backBufferPitch < outputWidth * 2 + 256)
					{
						bytesPerPixel = 2;
					}

					const std::uint32_t temporaryPitch = movieWidth * bytesPerPixel;
					const std::size_t requiredSize = static_cast<std::size_t>(temporaryPitch) * static_cast<std::size_t>(movieHeight);

					if (requiredSize == 0 || requiredSize > 8192ull * 8192ull * 4ull)
					{
						return;
					}

					if (s_instance_->m_binkFrameBuffer.size() != requiredSize)
					{
						s_instance_->m_binkFrameBuffer.resize(requiredSize);
					}

					s_instance_->m_binkBackBufferBits = backBufferBits;
					s_instance_->m_binkBackBufferPitch = backBufferPitch;
					s_instance_->m_binkMovieWidth = movieWidth;
					s_instance_->m_binkMovieHeight = movieHeight;
					s_instance_->m_binkBytesPerPixel = bytesPerPixel;
					s_instance_->m_binkTemporaryPitch = temporaryPitch;
					s_instance_->m_binkOutputWidth = outputWidth;
					s_instance_->m_binkOutputHeight = outputHeight;
					s_instance_->m_binkFramePending = true;

					*reinterpret_cast<std::uintptr_t*>(ctx.esp + 0x4) = reinterpret_cast<std::uintptr_t>(s_instance_->m_binkFrameBuffer.data());
					*reinterpret_cast<std::uint32_t*>(ctx.esp + 0x8) = temporaryPitch;
					*reinterpret_cast<std::uint32_t*>(ctx.esp + 0xC) = movieHeight;

					// The temporary buffer starts at its own top-left corner
					*reinterpret_cast<std::uint32_t*>(ctx.esp + 0x10) = 0;
					*reinterpret_cast<std::uint32_t*>(ctx.esp + 0x14) = 0;
				});
			}
			else
			{
				spdlog::info("Failed to locate BinkCopyToBuffer call instruction address.");
				return;
			}

			m_binkCopyReturnScanResult = Memory::PatternScan(m_LECDirect3DDllModule, "8B F8 A1 ?? ?? ?? ?? 50");
			if (m_binkCopyReturnScanResult)
			{
				spdlog::info("Bink Copy Return Instruction: Address is {:s}+{:x}", m_LECDirect3DDllModuleName, m_binkCopyReturnScanResult - reinterpret_cast<std::uint8_t*>(m_LECDirect3DDllModule));

				m_binkCopyReturnHook = safetyhook::create_mid(m_binkCopyReturnScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->ScaleBinkFrameToBackBuffer();
				});
			}
			else
			{
				spdlog::info("Failed to locate BinkCopyToBuffer return instruction address.");
				return;
			}

			m_letterboxUpdateMeshEndScanResult = Memory::PatternScan(m_LECGameDllModule, "5F 5E 5B C9 C3 D9 81 ?? ?? ?? ?? D8 74 24");
			if (m_letterboxUpdateMeshEndScanResult)
			{
				spdlog::info("Letterbox Geometry Instruction: Address is {:s}+{:x}", m_LECGameDllModuleName, m_letterboxUpdateMeshEndScanResult - reinterpret_cast<std::uint8_t*>(m_LECGameDllModule));

				m_letterboxGeometryHook = safetyhook::create_mid(m_letterboxUpdateMeshEndScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->CorrectLetterboxGeometry(ctx.esi);
				});
			}
			else
			{
				spdlog::info("Failed to locate the end of CLetterbox::UpdateMesh.");
				return;
			}

			// Located in CPanel::GetView() and CD3DRenderer::RenderStateOrtho() functions
			m_aspectRatioScansResult = Memory::PatternScan(m_LEC3DEngineDllModule, "DA 74 24", m_LECDirect3DDllModule, "E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 8D 55");
			if (Memory::AreAllSignaturesValid(m_aspectRatioScansResult))
			{
				spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", m_LEC3DEngineDllModuleName, m_aspectRatioScansResult[CameraAR] - (std::uint8_t*)m_LEC3DEngineDllModule);
				spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", m_LECDirect3DDllModuleName, m_aspectRatioScansResult[HUD_AR] - (std::uint8_t*)m_LECDirect3DDllModule);

				Memory::WriteNOPs(m_aspectRatioScansResult[CameraAR], 4);

				m_cameraAspectRatioHook = safetyhook::create_mid(m_aspectRatioScansResult[CameraAR], [](SafetyHookContext& ctx)
				{
					int& iCurrentResX2 = Memory::ReadMem(ctx.esp + 0x4);
					s_instance_->m_newResX2 = (int)(iCurrentResX2 * s_instance_->m_aspectRatioScale);
					FPU::FIDIV(s_instance_->m_newResX2);
				});

				m_hudAspectRatioHook = safetyhook::create_mid(m_aspectRatioScansResult[HUD_AR] + 5, [](SafetyHookContext& ctx)
				{
					*reinterpret_cast<float*>(ctx.ebp - 0x40) = *reinterpret_cast<float*>(ctx.ebp - 0x40) * s_instance_->m_hudScale;
					*reinterpret_cast<float*>(ctx.ebp - 0x10) = *reinterpret_cast<float*>(ctx.ebp - 0x10) * s_instance_->m_hudScale;
				});
			}

			m_blackBarsFillScanResult = Memory::PatternScan(m_LECDirect3DDllModule, "DB 06 8B 57 ?? 8D 0C 00");
			if (m_blackBarsFillScanResult)
			{
				spdlog::info("Black Bars Fill Instruction: Address is {:s}+{:x}", m_LECDirect3DDllModuleName, m_blackBarsFillScanResult - (std::uint8_t*)m_LECDirect3DDllModule);

				m_blackBarsFillHook = safetyhook::create_mid(m_blackBarsFillScanResult, [](SafetyHookContext& ctx)
				{
					ctx.eax = 0;
				});
			}
			else
			{
				spdlog::info("Failed to locate black bars fill instruction memory address.");
				return;
			}

			// Instructions are located in LEC3DEngine.CFrustum::SetupPerspective, LEC3DEngine.CView::LocalToScreen, LECGame.CViewManager::SetUnzoomedFOV, LECGame.CViewManager::SetZoomedFOV and LECDirect3D.CD3DRenderer::SetView
			m_cameraFOVScansResult = Memory::PatternScan(
				m_LEC3DEngineDllModule, "D9 46 ?? D8 0D", "D9 41 ?? D8 0D",
				m_LECGameDllModule, "8B 44 24 ?? A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? C2 ?? ?? D9 05 ?? ?? ?? ?? C3 8B 44 24", "8B 44 24 ?? A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? C2 ?? ?? D9 05 ?? ?? ?? ?? C3 53",
				m_LECDirect3DDllModule, "D9 46 ?? D8 0D"
			);
			if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
			{
				spdlog::info("Frustum FOV Instruction: Address is {:s}+{:x}", m_LEC3DEngineDllModuleName, m_cameraFOVScansResult[FrustumFOV] - (std::uint8_t*)m_LEC3DEngineDllModule);
				spdlog::info("Local to Screen FOV Instruction: Address is {:s}+{:x}", m_LECGameDllModuleName, m_cameraFOVScansResult[LocalToScreenFOV] - (std::uint8_t*)m_LECGameDllModule);
				spdlog::info("Unzoomed FOV Instruction: Address is {:s}+{:x}", m_LECGameDllModuleName, m_cameraFOVScansResult[UnzoomedFOV] - (std::uint8_t*)m_LECGameDllModule);
				spdlog::info("Zoomed FOV Instruction: Address is {:s}+{:x}", m_LECGameDllModuleName, m_cameraFOVScansResult[ZoomedFOV] - (std::uint8_t*)m_LECGameDllModule);
				spdlog::info("Renderer View FOV Instruction: Address is {:s}+{:x}", m_LECDirect3DDllModuleName, m_cameraFOVScansResult[RendererViewFOV] - (std::uint8_t*)m_LECDirect3DDllModule);

				Memory::WriteNOPs(m_cameraFOVScansResult[FrustumFOV], 3);

				m_frustumFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[FrustumFOV], [](SafetyHookContext& ctx)
				{
					s_instance_->m_currentFrustumFOV = Memory::ReadMem(ctx.esi + 0x78);
					s_instance_->m_newFrustumFOV = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentFrustumFOV, s_instance_->m_aspectRatioScale);
					FPU::FLD(s_instance_->m_newFrustumFOV);
				});

				Memory::WriteNOPs(m_cameraFOVScansResult[LocalToScreenFOV], 3);

				m_localToScreenFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[LocalToScreenFOV], [](SafetyHookContext& ctx)
				{
					float& fCurrentLocalToScreenFOV = Memory::ReadMem(ctx.ecx + 0x78);
					s_instance_->m_newLocalToScreenFOV = Maths::CalculateNewFOV_RadBased(fCurrentLocalToScreenFOV, s_instance_->m_aspectRatioScale);
					FPU::FLD(s_instance_->m_newLocalToScreenFOV);
				});

				Memory::WriteNOPs(m_cameraFOVScansResult[UnzoomedFOV], 4);

				m_unzoomedFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[UnzoomedFOV], [](SafetyHookContext& ctx)
				{
					float& fCurrentUnzoomedFOV = Memory::ReadMem(ctx.esp + 0x4);
					s_instance_->m_newUnzoomedFOV = fCurrentUnzoomedFOV * s_instance_->m_unzoomedFOVFactor;
					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newUnzoomedFOV);
				});

				Memory::WriteNOPs(m_cameraFOVScansResult[ZoomedFOV], 4);

				m_zoomedFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[ZoomedFOV], [](SafetyHookContext& ctx)
				{
					float& fCurrentZoomedFOV = Memory::ReadMem(ctx.esp + 0x4);
					s_instance_->m_newZoomedFOV = fCurrentZoomedFOV / s_instance_->m_zoomedFOVFactor;
					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newZoomedFOV);
				});

				Memory::WriteNOPs(m_cameraFOVScansResult[RendererViewFOV], 3);

				m_rendererViewFOVHook = safetyhook::create_mid(m_cameraFOVScansResult[RendererViewFOV], [](SafetyHookContext& ctx)
				{
					float& fCurrentRendererViewFOV = Memory::ReadMem(ctx.esi + 0x78);
					s_instance_->m_newRendererViewFOV = Maths::CalculateNewFOV_RadBased(fCurrentRendererViewFOV, s_instance_->m_aspectRatioScale);
					FPU::FLD(s_instance_->m_newRendererViewFOV);
				});
			}

			if (m_skipIntroVideos = true)
			{
				m_skipIntroVideosCheckScanResult = Memory::PatternScan(ExeModule(), "74 ?? 8D 45 ?? 8D 4D ?? 50 FF 35 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4E");
				if (m_skipIntroVideosCheckScanResult)
				{
					spdlog::info("Skip Intro Videos Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosCheckScanResult - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(m_skipIntroVideosCheckScanResult, "\xEB\x4E");
				}
				else
				{
					spdlog::info("Failed to locate skip intro videos check instruction memory address.");
					return;
				}
			}
		}		
	}

private:
	HMODULE m_LEC3DEngineDllModule = nullptr;
	HMODULE m_LECGameDllModule = nullptr;
	HMODULE m_LECDirect3DDllModule = nullptr;
	std::string m_LEC3DEngineDllModuleName = "";
	std::string m_LECGameDllModuleName = "";
	std::string m_LECDirect3DDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_skipIntroVideos = false;

	uint8_t* m_resolutionScanResult = nullptr;
	std::vector<uint8_t*> m_resolutionListUnlockScansResult{};
	uint8_t* m_uiModeResolutionScanResult = nullptr;
	uint8_t* m_uiModeViewportScanResult = nullptr;
	uint8_t* m_binkCopyScanResult = nullptr;
	uint8_t* m_binkCopyReturnScanResult = nullptr;
	uint8_t* m_blackBarsFillScanResult = nullptr;
	uint8_t* m_letterboxUpdateMeshEndScanResult = nullptr;	
	uint8_t* m_multipleInstancesCheckScanResult = nullptr;
	uint8_t* m_skipIntroVideosCheckScanResult = nullptr;
	std::vector<std::uint8_t*> m_aspectRatioScansResult{};
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	
	float m_currentFrustumFOV = 0.0f;
	float m_newFrustumFOV = 0.0f;
	float m_newLocalToScreenFOV = 0.0f;
	float m_newUnzoomedFOV = 0.0f;
	float m_newZoomedFOV = 0.0f;
	float m_newRendererViewFOV = 0.0f;

	float m_unzoomedFOVFactor = 0.0f;
	float m_zoomedFOVFactor = 0.0f;
	int m_newResX2 = 0;

	int m_binkOutputWidth = 0;
	int m_binkOutputHeight = 0;

	float m_hudScale = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_binkDisplayModeHook{};
	SafetyHookMid m_binkCopyHook{};
	SafetyHookMid m_binkCopyReturnHook{};
	SafetyHookMid m_cameraAspectRatioHook{};
	SafetyHookMid m_hudAspectRatioHook{};
	SafetyHookMid m_blackBarsFillHook{};
	SafetyHookMid m_letterboxGeometryHook{};
	SafetyHookMid m_uiModeViewportHook{};
	SafetyHookMid m_frustumFOVHook{};
	SafetyHookMid m_localToScreenFOVHook{};
	SafetyHookMid m_unzoomedFOVHook{};
	SafetyHookMid m_zoomedFOVHook{};
	SafetyHookMid m_rendererViewFOVHook{};

	void GetDLLModulesInfo()
	{
		m_LEC3DEngineDllModule = Memory::GetHandle("LEC3DEngine.dll");
		m_LECGameDllModule = Memory::GetHandle("LECGame.dll");
		m_LECDirect3DDllModule = Memory::GetHandle("LECDirect3D.dll");
		m_LEC3DEngineDllModuleName = Memory::GetModuleName(m_LEC3DEngineDllModule);
		m_LECGameDllModuleName = Memory::GetModuleName(m_LECGameDllModule);
		m_LECDirect3DDllModuleName = Memory::GetModuleName(m_LECDirect3DDllModule);
	}

	void CalculateARAndScale()
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
		m_hudScale = 1.0f / m_aspectRatioScale;
	}

	std::vector<std::uint8_t> m_binkFrameBuffer;

	std::uintptr_t m_binkBackBufferBits = 0;
	std::uint32_t m_binkBackBufferPitch = 0;

	std::uint32_t m_binkMovieWidth = 0;
	std::uint32_t m_binkMovieHeight = 0;
	std::uint32_t m_binkBytesPerPixel = 0;
	std::uint32_t m_binkTemporaryPitch = 0;

	bool m_binkFramePending = false;

	void ScaleBinkFrameToBackBuffer()
	{
		if (!m_binkFramePending || m_binkFrameBuffer.empty() || m_binkBackBufferBits == 0 || m_binkBackBufferPitch == 0 ||
			m_binkMovieWidth == 0 || m_binkMovieHeight == 0 || m_binkBytesPerPixel == 0 || m_binkOutputWidth <= 0 ||
			m_binkOutputHeight <= 0)
		{
			m_binkFramePending = false;
			return;
		}

		const std::uint32_t outputWidth = static_cast<std::uint32_t>(m_binkOutputWidth);
		const std::uint32_t outputHeight = static_cast<std::uint32_t>(m_binkOutputHeight);

		const float scaleX = static_cast<float>(outputWidth) / static_cast<float>(m_binkMovieWidth);
		const float scaleY = static_cast<float>(outputHeight) / static_cast<float>(m_binkMovieHeight);

		const float scale = std::min(scaleX, scaleY);

		const std::uint32_t scaledWidth = std::max(1u, static_cast<std::uint32_t>(static_cast<float>(m_binkMovieWidth) * scale + 0.5f));
		const std::uint32_t scaledHeight = std::max(1u, static_cast<std::uint32_t>(static_cast<float>(m_binkMovieHeight) * scale + 0.5f));

		const std::uint32_t destinationX = (outputWidth - scaledWidth) / 2;
		const std::uint32_t destinationY = (outputHeight - scaledHeight) / 2;

		const auto* const source = m_binkFrameBuffer.data();

		auto* const destination = reinterpret_cast<std::uint8_t*>(m_binkBackBufferBits);

		// Nearest-neighbor scaling
		for (std::uint32_t y = 0; y < scaledHeight; ++y)
		{
			const std::uint32_t sourceY = static_cast<std::uint32_t>((static_cast<std::uint64_t>(y) * m_binkMovieHeight) / scaledHeight);
			const std::uint8_t* const sourceRow = source + static_cast<std::size_t>(sourceY) * m_binkTemporaryPitch;
			std::uint8_t* const destinationRow = destination + static_cast<std::size_t>(destinationY + y) * m_binkBackBufferPitch + static_cast<std::size_t>(destinationX) * m_binkBytesPerPixel;

			if (m_binkBytesPerPixel == 4)
			{
				const auto* const sourcePixels = reinterpret_cast<const std::uint32_t*>(sourceRow);
				auto* const destinationPixels = reinterpret_cast<std::uint32_t*>(destinationRow);

				for (std::uint32_t x = 0; x < scaledWidth; ++x)
				{
					const std::uint32_t sourceX = static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * m_binkMovieWidth) / scaledWidth);
					destinationPixels[x] = sourcePixels[sourceX];
				}
			}
			else if (m_binkBytesPerPixel == 2)
			{
				const auto* const sourcePixels = reinterpret_cast<const std::uint16_t*>(sourceRow);
				auto* const destinationPixels = reinterpret_cast<std::uint16_t*>(destinationRow);

				for (std::uint32_t x = 0; x < scaledWidth; ++x)
				{
					const std::uint32_t sourceX = static_cast<std::uint32_t>((static_cast<std::uint64_t>(x) * m_binkMovieWidth) / scaledWidth);
					destinationPixels[x] = sourcePixels[sourceX];
				}
			}
		}

		m_binkFramePending = false;
	}

	void CorrectLetterboxGeometry(std::uintptr_t letterboxObject)
	{
		if (letterboxObject == 0 || m_hudScale <= 0.0f)
		{
			return;
		}

		const float logicalWidth = *reinterpret_cast<const float*>(letterboxObject + 0x78);

		if (logicalWidth <= 0.0f || logicalWidth > 16384.0f)
		{
			return;
		}

		const std::uintptr_t subMesh = *reinterpret_cast<const std::uintptr_t*>(letterboxObject + 0xA4);

		if (subMesh == 0)
		{
			return;
		}

		const float expandedWidth = logicalWidth / m_hudScale;
		const float centerX = logicalWidth * 0.5f;

		// Extend slightly beyond the calculated screen boundaries to compensate for Direct3D 8 pixel-center rasterization and floating-point rounding errors
		constexpr float horizontalOverscan = 1.0f;

		const float left =	centerX - expandedWidth * 0.5f - horizontalOverscan;
		const float right = centerX + expandedWidth * 0.5f + horizontalOverscan;

		using GetVertexFunction = float* (__thiscall*)(void*, std::uint32_t);

		auto* const virtualTable = *reinterpret_cast<std::uintptr_t**>(subMesh);

		if (virtualTable == nullptr)
		{
			return;
		}

		const auto getVertex = reinterpret_cast<GetVertexFunction>(virtualTable[0x10 / sizeof(std::uintptr_t)]);

		if (getVertex == nullptr)
		{
			return;
		}

		constexpr std::uint32_t leftVertices[] =
		{
			0, 1, 4, 5
		};

		constexpr std::uint32_t rightVertices[] =
		{
			2, 3, 6, 7
		};

		for (const std::uint32_t vertexIndex : leftVertices)
		{
			float* const vertex = getVertex(reinterpret_cast<void*>(subMesh), vertexIndex);

			if (vertex != nullptr)
			{
				vertex[0] = left;
			}
		}

		for (const std::uint32_t vertexIndex : rightVertices)
		{
			float* const vertex = getVertex(reinterpret_cast<void*>(subMesh), vertexIndex);

			if (vertex != nullptr)
			{
				vertex[0] = right;
			}
		}
	}

	struct D3DViewport8
	{
		std::uint32_t X;
		std::uint32_t Y;
		std::uint32_t Width;
		std::uint32_t Height;
		float MinZ;
		float MaxZ;
	};

	using HRESULT32 = std::int32_t;

	static HRESULT32 SetD3D8Viewport(std::uintptr_t device, const D3DViewport8& viewport)
	{
		if (device == 0)
		{
			return static_cast<HRESULT32>(0x80004003u);
		}

		auto* const vtable = *reinterpret_cast<std::uintptr_t**>(device);

		if (vtable == nullptr)
		{
			return static_cast<HRESULT32>(0x80004003u);
		}

		using SetViewportFunction = HRESULT32(__stdcall*)(void* device, const D3DViewport8* viewport);

		const auto setViewport = reinterpret_cast<SetViewportFunction>(vtable[40]);

		if (setViewport == nullptr)
		{
			return static_cast<HRESULT32>(0x80004003u);
		}

		return setViewport(reinterpret_cast<void*>(device), &viewport);
	}

	enum ResolutionListUnlockInstructionsIndices
	{
		ListUnlock1,
		ListUnlock2,
		ListUnlock3
	};

	enum CameraFOVInstructionsIndices
	{
		FrustumFOV,
		LocalToScreenFOV,
		UnzoomedFOV,
		ZoomedFOV,
		RendererViewFOV
	};

	enum AspectRatioInstructionsIndices
	{
		CameraAR,
		HUD_AR
	};

	inline static StarWarsStarfighterFix* s_instance_ = nullptr;
};

static std::unique_ptr<StarWarsStarfighterFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<StarWarsStarfighterFix>(hModule);
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