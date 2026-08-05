#include "..\..\common\FixBase.hpp"

class Torrente3Fix final : public FixBase
{
public:
	explicit Torrente3Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~Torrente3Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Torrente3TheProtectorWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Torrente 3: The Protector";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Torrente3.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		m_resolutionListsScansResult = Memory::PatternScan(ExeModule(), "BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? 8D 54 24",
		"BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? EB ?? BE ?? ?? ?? ?? BF ?? ?? ?? ?? 39 2D");
		if (Memory::AreAllSignaturesValid(m_resolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListsScansResult[List1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionListsScansResult[List2] - (std::uint8_t*)ExeModule());

			// Resolution List 1
			// 640x480
			Memory::Write(m_resolutionListsScansResult[List1] + 1, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List1] + 6, m_newResY);
			// 800x600
			Memory::Write(m_resolutionListsScansResult[List1] + 13, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List1] + 18, m_newResY);
			// 1280x1024
			Memory::Write(m_resolutionListsScansResult[List1] + 25, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List1] + 30, m_newResY);
			// 1024x768
			Memory::Write(m_resolutionListsScansResult[List1] + 37, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List1] + 42, m_newResY);

			// Resolution List 2
			// 640x480
			Memory::Write(m_resolutionListsScansResult[List2] + 1, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List2] + 6, m_newResY);
			// 800x600
			Memory::Write(m_resolutionListsScansResult[List2] + 13, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List2] + 18, m_newResY);
			// 1280x1024
			Memory::Write(m_resolutionListsScansResult[List2] + 25, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List2] + 30, m_newResY);
			// 1024x768
			Memory::Write(m_resolutionListsScansResult[List2] + 37, m_newResX);
			Memory::Write(m_resolutionListsScansResult[List2] + 42, m_newResY);
		}

		m_aspectRatioScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50");
		if (m_aspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScanResult - (std::uint8_t*)ExeModule());

			m_newAspectRatio2 = 0.75f / m_aspectRatioScale;

			Memory::Write(m_aspectRatioScanResult + 1, m_newAspectRatio2);
		}
		else
		{
			spdlog::info("Failed to locate the aspect ratio instruction memory address.");
			return;
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "F3 0F 10 05 ?? ?? ?? ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? 8B 56 ?? 8B 8A ?? ?? ?? ??", "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 8E ?? ?? ?? ?? 8B 49 ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ??",
		"F3 0F 10 05 ?? ?? ?? ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? 8B 4E ??", "F3 0F 10 05 ?? ?? ?? ?? 0F 2E 41 ?? 9F F6 C4 ?? 7B ?? F3 0F 11 41 ?? E8 ?? ?? ?? ?? 8B 46 ?? 8B 48 ??", "F3 0F 10 05 ?? ?? ?? ?? 89 48 ?? 8B 7F ?? 0F 2E 47 ??", "F3 0F 10 44 24 ?? 83 C4 ?? 0F 2E 41 ?? 9F F6 C4 ??", "F3 0F 10 05 ?? ?? ?? ?? 89 48 ?? 8B 4F ?? EB ?? 84 C0 0F 85 ?? ?? ?? ??",
		"F3 0F 10 05 ?? ?? ?? ?? 8B 56 ?? F3 0F 11 86 ?? ?? ?? ?? 89 4E ?? 8B 82 ?? ?? ?? ?? C1 E8 ?? A8 ??", "F3 0F 10 05 ?? ?? ?? ?? 8D 4C 24 ?? 51 6A ?? 8D 54 24 ?? 52 8D 44 24 ??", "F3 0F 10 05 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 46 ?? 89 46 ?? 89 46 ??", "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? 8B 76 ??", "F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 42 ?? 8B 51 ?? 88 41 ??",
		"F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 86 ?? ?? ?? ?? F3 0F 10 05 ?? ?? ?? ?? F3 0F 11 46 ?? F3 0F 10 05 ?? ?? ?? ?? 5F");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV7] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV8] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV9] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV10] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 11: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV11] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 12: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV12] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 13: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV13] - (std::uint8_t*)ExeModule());

			m_cameraFOV1Address = Memory::GetPointerFromAddress(m_cameraFOVScansResult[FOV1] + 4, Memory::PointerMode::Absolute);
			m_cameraFOV2Address = Memory::GetPointerFromAddress(m_cameraFOVScansResult[FOV3] + 4, Memory::PointerMode::Absolute);
			m_cameraFOV3Address = Memory::GetPointerFromAddress(m_cameraFOVScansResult[FOV5] + 4, Memory::PointerMode::Absolute);
			m_cameraFOV4Address = Memory::GetPointerFromAddress(m_cameraFOVScansResult[FOV7] + 4, Memory::PointerMode::Absolute);
			m_cameraFOV5Address = Memory::GetPointerFromAddress(m_cameraFOVScansResult[FOV13] + 4, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV1], 8);

			m_cameraFOV1Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV2], 8);

			m_cameraFOV2Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV3], 8);

			m_cameraFOV3Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV2Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV4], 8);

			m_cameraFOV4Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV4], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV5], 8);

			m_cameraFOV5Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV5], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV3Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV6], 6);

			m_cameraFOV6Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV6], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(ctx.esp + 0x44, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV7], 8);

			m_cameraFOV7Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV7], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV4Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV8], 8);

			m_cameraFOV8Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV8], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV9], 8);

			m_cameraFOV9Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV9], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV10], 8);

			m_cameraFOV10Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV10], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV11], 8);

			m_cameraFOV11Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV11], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV12], 8);

			m_cameraFOV12Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV12], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV1Address, ctx.xmm0.f32[0], ctx);
			});

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV13], 8);

			m_cameraFOV13Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV13], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(s_instance_->m_cameraFOV5Address, ctx.xmm0.f32[0], ctx);
			});
		}

		m_windowNameScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 50 6A ?? FF 15", "68 ?? ?? ?? ?? 50 55 FF 15");
		if (Memory::AreAllSignaturesValid(m_windowNameScansResult) == true)
		{
			spdlog::info("Window Name Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_windowNameScansResult[WindowName1] - (std::uint8_t*)ExeModule());
			spdlog::info("Window Name Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), m_windowNameScansResult[WindowName2] - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(m_windowNameScansResult[WindowName1] + 1, m_newWindowName);
			Memory::PatchBytes(m_windowNameScansResult[WindowName2] + 1, m_newWindowName);
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "6A ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A ?? 6A ?? 81 C1 ?? ?? ?? ?? E8 ?? ?? ?? ?? 5E",
			"73 48 F3 0F 10 15 ?? ?? ?? ??");
			if (Memory::AreAllSignaturesValid(m_skipIntroVideosScansResult) == true)
			{
				spdlog::info("Main Menu Event Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScansResult[MainMenuEvent] - (std::uint8_t*)ExeModule());
				spdlog::info("Legal Screen Timer Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideosScansResult[LegalScreenTimer] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_skipIntroVideosScansResult[MainMenuEvent] + 1, "\x01");
				Memory::PatchBytes(m_skipIntroVideosScansResult[LegalScreenTimer], "\xEB");
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	const char* m_newWindowName = "Torrente 3: The Protector";

	bool m_skipIntroVideos = false;

	std::vector<std::uint8_t*> m_resolutionListsScansResult{};
	std::uint8_t* m_aspectRatioScanResult = nullptr;
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::vector<std::uint8_t*> m_windowNameScansResult{};
	std::vector<std::uint8_t*> m_skipIntroVideosScansResult{};

	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};
	SafetyHookMid m_cameraFOV5Hook{};
	SafetyHookMid m_cameraFOV6Hook{};
	SafetyHookMid m_cameraFOV7Hook{};
	SafetyHookMid m_cameraFOV8Hook{};
	SafetyHookMid m_cameraFOV9Hook{};
	SafetyHookMid m_cameraFOV10Hook{};
	SafetyHookMid m_cameraFOV11Hook{};
	SafetyHookMid m_cameraFOV12Hook{};
	SafetyHookMid m_cameraFOV13Hook{};

	enum ResolutionsInstructionsIndices
	{
		List1,
		List2
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7,
		FOV8,
		FOV9,
		FOV10,
		FOV11,
		FOV12,
		FOV13
	};

	enum SkipIntroVideosInstructionsIndices
	{
		MainMenuEvent,
		LegalScreenTimer
	};

	enum WindowNameInstructionsIndices
	{
		WindowName1,
		WindowName2
	};

	uintptr_t m_cameraFOV1Address = 0;
	uintptr_t m_cameraFOV2Address = 0;
	uintptr_t m_cameraFOV3Address = 0;
	uintptr_t m_cameraFOV4Address = 0;
	uintptr_t m_cameraFOV5Address = 0;

	void CameraFOVsMidHook(uintptr_t FOVAddress, float& DestInstruction, SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);
		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;
		DestInstruction = m_newCameraFOV;
	}

	inline static Torrente3Fix* s_instance_ = nullptr;
};

static std::unique_ptr<Torrente3Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<Torrente3Fix>(hModule);
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