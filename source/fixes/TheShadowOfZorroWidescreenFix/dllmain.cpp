#include "..\..\common\FixBase.hpp"

class ShadowOfZorroFix final : public FixBase
{
public:
	explicit ShadowOfZorroFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ShadowOfZorroFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheShadowOfZorroWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "The Shadow of Zorro";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "video.exe") ||
		Util::stringcmp_caseless(exeName, "Zorro.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "video.exe"))
		{
			auto ResolutionListUnlockScansResult = Memory::PatternScan(ExeModule(), "7F ?? 3B 44 24", "7F ?? 47 3B C7");
			if (Memory::AreAllSignaturesValid(ResolutionListUnlockScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScansResult[ListUnlock1], 2);
				Memory::WriteNOPs(ResolutionListUnlockScansResult[ListUnlock2], 2);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Zorro.exe"))
		{
			m_fnxCoreModule = Memory::GetHandle("fnx_core.dll");
			m_fnxCoreModuleName = Memory::GetModuleName(m_fnxCoreModule);
			m_fnxDX7Module = Memory::GetHandle("fnx_dx7.dll");
			m_fnxDX7ModuleName = Memory::GetModuleName(m_fnxDX7Module);

			ResolutionScans2Result = Memory::PatternScan(m_fnxDX7Module, "FF 51 ?? 85 C0 0F 85 ?? ?? ?? ?? 6A", ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 8D 4C 24 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 8B 15",
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 8D 4C 24 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 85 F6", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 75 ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 50",
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0 75 ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53",
			"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53 8D 54 24");
			if (Memory::AreAllSignaturesValid(ResolutionScans2Result) == true)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), ResolutionScans2Result[ResolutionWidthHeight] - (std::uint8_t*)m_fnxDX7Module);
				spdlog::info("Cryo Logo Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans2Result[ResolutionCryoLogo] - (std::uint8_t*)ExeModule());
				spdlog::info("In Utero Logo Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans2Result[ResolutionInUteroLogo] - (std::uint8_t*)ExeModule());
				spdlog::info("Startup Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans2Result[ResolutionStartup1] - (std::uint8_t*)ExeModule());
				spdlog::info("Startup Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans2Result[ResolutionStartup2] - (std::uint8_t*)ExeModule());
				spdlog::info("Startup Resolution Instructions 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans2Result[ResolutionStartup3] - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(ResolutionScans2Result[ResolutionWidthHeight], [](SafetyHookContext& ctx)
				{
					s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x4);
					s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x8);
					s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
					s_instance_->WriteIntroVideosRes();
					s_instance_->m_resolutionHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}

			auto AspectRatioScanResult = Memory::PatternScan(m_fnxCoreModule, "A1 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B C6");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", m_fnxCoreModuleName.c_str(), AspectRatioScanResult - (std::uint8_t*)m_fnxCoreModule);

				Memory::WriteNOPs(AspectRatioScanResult, 5);

				m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
				{
					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio);
				});
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction memory address.");
				return;
			}

			auto CameraFOVScansResult = Memory::PatternScan(m_fnxDX7Module, "8B 8E ?? ?? ?? ?? 52", "D9 87 ?? ?? ?? ?? 8B CF");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)m_fnxDX7Module);
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)m_fnxDX7Module);

				Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

				m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.esi + 0xCC);
					s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
					ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV1);
				});

				Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

				m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.edi + 0xCC);
					s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale);
					FPU::FLD(s_instance_->m_newCameraFOV2);
				});
			}
			
			auto InputFixScansResult = Memory::PatternScan(ExeModule(), "3D ?? ?? ?? ?? 75 ?? 8B 46", "3D ?? ?? ?? ?? 75 ?? 8B 43 ?? 50 8B 08 FF 51 ?? 85 C0 7D ?? BF");
			if (Memory::AreAllSignaturesValid(InputFixScansResult) == true)
			{
				spdlog::info("Input fix: Mouse GetDeviceState error check found at {:s}+{:x}", ExeName().c_str(), InputFixScansResult[MouseErrorCheck] - (std::uint8_t*)(ExeModule()));
				spdlog::info("Input fix: Keyboard GetDeviceState error check found at {:s}+{:x}", ExeName().c_str(), InputFixScansResult[KeyboardErrorCheck] - (std::uint8_t*)(ExeModule()));

				Memory::PatchBytes(InputFixScansResult[MouseErrorCheck], "\x85\xC0\x90\x90\x90\x79\x18");
				Memory::PatchBytes(InputFixScansResult[KeyboardErrorCheck], "\x85\xC0\x90\x90\x90\x79\x0D");
			}

			if (m_skipIntroVideos == true)
			{
				auto SkipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "74 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 8D 4C 24 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 8B 15",
				"74 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A ?? 8D 4C 24 ?? 6A ?? 51 8B CE E8 ?? ?? ?? ?? 85 F6");
				if (Memory::AreAllSignaturesValid(SkipIntroVideosScansResult) == true)
				{
					spdlog::info("Cryo Logo Intro Video Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[IntroCryoLogo] - (std::uint8_t*)ExeModule());
					spdlog::info("In Utero Logo Intro Video Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[IntroInUteroLogo] - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(SkipIntroVideosScansResult[IntroCryoLogo], "\xEB");
					Memory::PatchBytes(SkipIntroVideosScansResult[IntroInUteroLogo], "\xEB");
				}
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	HMODULE m_fnxDX7Module = nullptr;
	std::string m_fnxDX7ModuleName = "";
	HMODULE m_fnxCoreModule = nullptr;
	std::string m_fnxCoreModuleName = "";

	bool m_skipIntroVideos = false;

	std::vector<std::uint8_t*> ResolutionScans2Result;

	int m_newDefaultWidth = 0;
	int m_newDefaultHeight = 0;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum Resolution1
	{
		ListUnlock1,
		ListUnlock2
	};

	enum ResolutionScanIndex
	{
		ResolutionWidthHeight,
		ResolutionCryoLogo,
		ResolutionInUteroLogo,
		ResolutionStartup1,
		ResolutionStartup2,
		ResolutionStartup3
	};

	enum InputFixScanIndex
	{
		MouseErrorCheck,
		KeyboardErrorCheck
	};

	enum IntroVideoScanIndex
	{
		IntroCryoLogo,
		IntroInUteroLogo
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2
	};

	void WriteIntroVideosRes()
	{
		Memory::Write(ResolutionScans2Result, ResolutionCryoLogo, ResolutionStartup3, 6, m_newResX);
		Memory::Write(ResolutionScans2Result, ResolutionCryoLogo, ResolutionStartup3, 1, m_newResY);
	}

	inline static ShadowOfZorroFix* s_instance_ = nullptr;
};

static std::unique_ptr<ShadowOfZorroFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<ShadowOfZorroFix>(hModule);
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