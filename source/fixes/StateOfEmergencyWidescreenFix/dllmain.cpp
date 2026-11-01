#include "..\..\common\FixBase.hpp"

class StateOfEmergencyFix final : public FixBase
{
public:
	explicit StateOfEmergencyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~StateOfEmergencyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "StateOfEmergencyWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.8.2";
	}

	const char* TargetName() const override
	{
		return "State of Emergency";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "KaosPC.exe");
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
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "F6 C4 ?? 7A ?? 8B 44 24 ?? 45", "F6 C4 ?? 7A ?? 47", "8B 04 10 57 A3");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan Instruction: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock1], 5);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock2], 5);

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentWidth = Memory::ReadMem(ctx.eax + ctx.edx);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight] + 19, [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHeight = Memory::ReadMem(ctx.eax + ctx.ecx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_currentWidth) / static_cast<float>(s_instance_->m_currentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? 75 ?? C7 44 24 ?? ?? ?? ?? ?? D9 44 24 ?? 8B 44 24",
		"D9 05 ?? ?? ?? ?? D8 4C 24 ?? C7 05");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR] + 10 - (std::uint8_t*)ExeModule());
			spdlog::info("HFOV Culling Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[HFOVCulling] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[AR], 8);
			Memory::WriteNOPs(AspectRatioScansResult[AR] + 10, 8);
			Memory::WriteNOPs(AspectRatioScansResult[HFOVCulling], 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esp + 0x48) = s_instance_->m_newAspectRatio;
			});

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR] + 10, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<float*>(ctx.esp + 0x48) = s_instance_->m_newAspectRatio;
			});

			m_hfovCullingHook = safetyhook::create_mid(AspectRatioScansResult[HFOVCulling], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newHFOVCulling = 1.0f / s_instance_->m_aspectRatioScale;

				FPU::FLD(s_instance_->m_newHFOVCulling);
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? E8", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53 C7 05");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult))
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());

			m_newCameraFOV1 = 810.0f / m_fovFactor;

			m_newCameraFOV2 = 900.0f / m_fovFactor;

			Memory::Write(CameraFOVScansResult[FOV1] + 1, m_newCameraFOV1);
			Memory::Write(CameraFOVScansResult[FOV2] + 1, m_newCameraFOV1);
			Memory::Write(CameraFOVScansResult[FOV3] + 1, m_newCameraFOV2);			
		}

		auto HUDScansResult = Memory::PatternScan(ExeModule(), "8B 54 24 ?? D9 44 24 ?? D9 1A", "8B 44 24 ?? C7 00 ?? ?? ?? ?? 8B 44 24",
		"A1 ?? ?? ?? ?? 8B 08 50 FF 51 ?? 85 C0 7D ?? 50 68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 83 C4 ?? FF D7", "FF 91 ?? ?? ?? ?? A1 ?? ?? ?? ?? 85 C0 BE", "39 35 ?? ?? ?? ?? 7E ?? 33 DB",
		"A1 ?? ?? ?? ?? 85 C0 75 ?? 8B 44 24 ?? 8B 4C 24");
		if (Memory::AreAllSignaturesValid(HUDScansResult))
		{
			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUDQuadNormalPath] - (std::uint8_t*)ExeModule());
			spdlog::info("Fullscreen Quad Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[FullscreenBufferCapture] - (std::uint8_t*)ExeModule());
			spdlog::info("Textured Fullscreen Draw Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[TexturedFullscreenDraw] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Batch Instruction: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[HUDBatchCheck] - (std::uint8_t*)ExeModule());
			spdlog::info("Submit Quad Function: Address is {:s}+{:x}", ExeName().c_str(), HUDScansResult[SubmitQuad] - (std::uint8_t*)ExeModule());

			m_submitQuad = reinterpret_cast<SubmitQuadFn>(HUDScansResult[SubmitQuad]);

			m_hudAspectHook = safetyhook::create_mid(HUDScansResult[HUDQuadNormalPath], [](SafetyHookContext& ctx)
			{
				if (s_instance_->m_aspectRatioScale <= 1.0f)
				{
					return;
				}

				auto& x1 = *reinterpret_cast<float*>(ctx.esp + 0x14);
				auto& y1 = *reinterpret_cast<float*>(ctx.esp + 0x18);
				auto& x2 = *reinterpret_cast<float*>(ctx.esp + 0x1C);
				auto& y2 = *reinterpret_cast<float*>(ctx.esp + 0x20);

				constexpr float epsilon = 0.001f;

				// Leave black and white overlays at 0 or 1 so they continue covering the complete physical display rather than the centered 4:3 area
				const bool isFullscreen = x1 <= epsilon && y1 <= epsilon && x2 >= 1.0f - epsilon && y2 >= 1.0f - epsilon;

				if (isFullscreen)
				{
					return;
				}

				const float correction = 1.0f / s_instance_->m_aspectRatioScale;

				x1 = 0.5f + ((x1 - 0.5f) * correction);
				x2 = 0.5f + ((x2 - 0.5f) * correction);
			});

			m_fullscreenBufferCaptureHook = safetyhook::create_mid(HUDScansResult[FullscreenBufferCapture], [](SafetyHookContext& ctx)
			{
				s_instance_->m_fullscreenVertices = *reinterpret_cast<std::uintptr_t*>(ctx.esp + 0x04);
			});

			m_fullscreenQuadHook = safetyhook::create_mid(HUDScansResult[TexturedFullscreenDraw], [](SafetyHookContext&)
			{
				if (s_instance_->m_fullscreenVertices == 0 || s_instance_->m_aspectRatioScale <= 0.0f)
				{
					return;
				}

				const float correction = 1.0f / s_instance_->m_aspectRatioScale;

				if (correction >= 1.0f)
				{
					return;
				}

				const float left = (1.0f - correction) * 0.5f;
				const float right = 1.0f - left;

				std::uint8_t* vertices = reinterpret_cast<std::uint8_t*>(s_instance_->m_fullscreenVertices);

				*reinterpret_cast<float*>(vertices + 0x00) = left;
				*reinterpret_cast<float*>(vertices + 0x14) = right;
				*reinterpret_cast<float*>(vertices + 0x28) = right;
				*reinterpret_cast<float*>(vertices + 0x3C) = left;
			});

			m_fullscreenDrawHook = safetyhook::create_mid(HUDScansResult[TexturedFullscreenDraw] + 6, [](SafetyHookContext&)
			{
				s_instance_->m_texturedFullscreenSeen = true;
			});

			// Add black side bars immediately before the HUD batch is rendered
			m_pillarboxHook = safetyhook::create_mid(HUDScansResult[HUDBatchCheck], [](SafetyHookContext&)
			{
				if (!s_instance_->m_texturedFullscreenSeen)
				{
					return;
				}

				// Consume the flag for this frame
				s_instance_->m_texturedFullscreenSeen = false;

				if (s_instance_->m_submitQuad == nullptr)
				{
					return;
				}

				const float correction = 1.0f / s_instance_->m_aspectRatioScale;

				if (correction >= 1.0f || correction <= 0.0f)
				{
					return;
				}

				// These coordinates pass through m_hudAspectHook, so we just use inverse-corrected outer edges
				//
				// HUD correction:
				// output = 0.5 + ((input - 0.5) * correction)
				//
				// Solved for input:
				// input = 0.5 + ((output - 0.5) / correction)
				const float leftOuter = 0.5f - (0.5f / correction);
				const float rightOuter = 0.5f + (0.5f / correction);
				constexpr float leftInner = 0.0f;
				constexpr float rightInner = 1.0f;
				constexpr float top = 0.0f;
				constexpr float bottom = 1.0f;

				// Left pillarbox
				s_instance_->m_submitQuad(nullptr, 0, leftOuter, top, leftInner, bottom, &s_instance_->m_pillarboxColor);

				// Right pillarbox
				s_instance_->m_submitQuad(nullptr, 0, rightInner, top, rightOuter, bottom, &s_instance_->m_pillarboxColor);
			});
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}

		if (m_skipIntroVideos == true)
		{
			auto SkipIntroVideoScanResult = Memory::PatternScan(ExeModule(), "A1 ?? ?? ?? ?? A8 ?? 75 ?? F6 C4 ?? EB", "A1 ?? ?? ?? ?? 75 ?? 3D",
			"75 ?? B8 ?? ?? ?? ?? 66 C7 05", "D9 05 ?? ?? ?? ?? 3C");
			if (Memory::AreAllSignaturesValid(SkipIntroVideoScanResult) == true)
			{
				spdlog::info("Startup Intro Video Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[StartupVid1] - (std::uint8_t*)ExeModule());
				spdlog::info("Startup Intro Video Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[StartupVid2] - (std::uint8_t*)ExeModule());
				spdlog::info("Inactivity Timer Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[InactivityTimerCheck] - (std::uint8_t*)ExeModule());
				spdlog::info("Inactivity Timer Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideoScanResult[InactivityTimer] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(SkipIntroVideoScanResult[StartupVid1], "\xE9\x24\x00\x00\x00");
				Memory::PatchBytes(SkipIntroVideoScanResult[StartupVid2], "\xB8\xFF\xFF\xFF\x7F");
				Memory::PatchBytes(SkipIntroVideoScanResult[InactivityTimerCheck], "\xEB");

				m_inactivityTimerAddress = Memory::GetPointerFromAddress(SkipIntroVideoScanResult[InactivityTimer] + 2, Memory::PointerMode::Absolute);
				
				m_inactivityTimerHook = safetyhook::create_mid(SkipIntroVideoScanResult[InactivityTimer], [](SafetyHookContext& ctx)
				{
					Memory::Write(s_instance_->m_inactivityTimerAddress, m_newInactivityTimerDuration);
				});
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_newInactivityTimerDuration = 0.0f;

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	uintptr_t m_inactivityTimerAddress = 0;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_hfovCullingHook{};
	SafetyHookMid m_inactivityTimerHook{};
	SafetyHookMid m_hudAspectHook{};
	SafetyHookMid m_fullscreenBufferCaptureHook{};
	SafetyHookMid m_fullscreenQuadHook{};
	SafetyHookMid m_fullscreenDrawHook{};
	SafetyHookMid m_pillarboxHook{};

	uint32_t m_currentWidth = 0;
	uint32_t m_currentHeight = 0;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;
	float m_newHFOVCulling = 0.0f;

	struct QuadColor
	{
		std::int32_t r;
		std::int32_t g;
		std::int32_t b;
		std::int32_t a;
	};

	using SubmitQuadFn = void(__cdecl*)(void* resource, std::uint32_t resourceIndex, float x1, float y1, float x2, float y2, QuadColor* colour);

	std::uintptr_t m_fullscreenVertices = 0;

	SubmitQuadFn m_submitQuad = nullptr;

	QuadColor m_pillarboxColor{ 0, 0, 0, 255 };

	bool m_texturedFullscreenSeen = false;

	enum ResolutionListsIndex
	{
		ListUnlock1,
		ListUnlock2,
		WidthHeight
	};

	enum AspectRatioInstructionsIndex
	{
		AR,
		HFOVCulling
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2,
		FOV3
	};

	enum SkipIntroVideosInstructionsIndex
	{
		StartupVid1,
		StartupVid2,
		InactivityTimerCheck,
	    InactivityTimer
	};

	enum HUDScansIndex
	{
		HUDQuadNormalPath,
		FullscreenBufferCapture,
		FullscreenFinalWrite,
		TexturedFullscreenDraw,
		HUDBatchCheck,
		SubmitQuad
	};

	inline static StateOfEmergencyFix* s_instance_ = nullptr;
};

static std::unique_ptr<StateOfEmergencyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<StateOfEmergencyFix>(hModule);
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