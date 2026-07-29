#include "..\..\common\FixBase.hpp"

class JekyllFix final : public FixBase
{
public:
	explicit JekyllFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~JekyllFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "Jekyll&HydeWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Jekyll & Hyde";
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
		if (Util::stringcmp_caseless(ExeName(), "video.exe"))
		{
			auto ResolutionScans1Result = Memory::PatternScan(ExeModule(), "3D ?? ?? ?? ?? 75 ?? 8B 16 8B CE FF 52 ?? 3D ?? ?? ?? ?? 0F 84",
			"3D ?? ?? ?? ?? 75 ?? 81 7C 24");
			if (Memory::AreAllSignaturesValid(ResolutionScans1Result) == true)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans1Result[ListUnlock] - (std::uint8_t*)ExeModule());
				spdlog::info("Default Resolution Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans1Result[DefaultRes] - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(ResolutionScans1Result[ListUnlock], "\xE9\xA8\x00\x00\x00");

				m_newDefaultWidth = Screen::GetDesktopResolutionWidth();
				m_newDefaultHeight = Screen::GetDesktopResolutionHeight();
				Memory::Write(ResolutionScans1Result[DefaultRes] + 1, m_newDefaultWidth);
				Memory::Write(ResolutionScans1Result[DefaultRes] + 11, m_newDefaultHeight);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Game.exe"))
		{
			m_fnxCoreModule = Memory::GetHandle("fnx_core.dll");
			m_fnxCoreModuleName = Memory::GetModuleName(m_fnxCoreModule);
			m_fnxDX7Module = Memory::GetHandle("fnx_dx7.dll");
			m_fnxDX7ModuleName = Memory::GetModuleName(m_fnxDX7Module);
			m_4xmSdkDllModule = Memory::GetHandle("4xm_sdk.dll");
			m_4xmSdkDllModuleName = Memory::GetModuleName(m_4xmSdkDllModule);

			auto ResolutionScans2Result = Memory::PatternScan(m_fnxDX7Module, "FF 51 ?? 85 C0 0F 85 ?? ?? ?? ?? 6A", ExeModule(), "E8 ?? ?? ?? ?? 50 B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 85 C0",
			m_4xmSdkDllModule, "8B 44 24 ?? 48");
			if (Memory::AreAllSignaturesValid(ResolutionScans2Result) == true)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), ResolutionScans2Result[WidthHeight] - (std::uint8_t*)m_fnxDX7Module);
				spdlog::info("FMVs Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScans2Result[FMVs] - (std::uint8_t*)ExeModule());
				spdlog::info("FMVs Scale Instructions Scan: Address is {:s}+{:x}", m_4xmSdkDllModuleName.c_str(), ResolutionScans2Result[FMVsScale] - (std::uint8_t*)m_4xmSdkDllModule);

				m_resolutionHook = safetyhook::create_mid(ResolutionScans2Result[WidthHeight], [](SafetyHookContext& ctx)
				{
					s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0x4);
					s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x8);
					s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
					s_instance_->m_resolutionHook.disable();
				});

				m_fmvsResolutionHook = safetyhook::create_mid(ResolutionScans2Result[FMVs], [](SafetyHookContext& ctx)
				{
					if (s_instance_->m_newResX < 640 || s_instance_->m_newResY < 480)
					{
						return;
					}

					auto& displayWidth = *reinterpret_cast<std::uint32_t*>(ctx.esp + 0x00);
					auto& displayHeight = *reinterpret_cast<std::uint32_t*>(ctx.esp + 0x04);
					auto& displayBitDepth = *reinterpret_cast<std::uint32_t*>(ctx.esp + 0x08);

					displayWidth = static_cast<std::uint32_t>(s_instance_->m_newResX);
					displayHeight = static_cast<std::uint32_t>(s_instance_->m_newResY);
					displayBitDepth = 16;
				});				

				m_fmvsScaleHook = safetyhook::create_mid(ResolutionScans2Result[FMVsScale], [](SafetyHookContext& ctx)
				{
					constexpr std::uint32_t nativeVideoWidth = 640;
					constexpr std::uint32_t nativeVideoHeight = 480;
					constexpr std::uint32_t bytesPerPixel = 2;

					if (s_instance_->m_newResX < nativeVideoWidth || s_instance_->m_newResY < nativeVideoHeight)
					{
						return;
					}

					auto* destinationArgument = reinterpret_cast<std::uint32_t*>(ctx.esp + 0x04);

					if (!destinationArgument || *destinationArgument == 0)
					{
						return;
					}

					const auto destinationPitch = *reinterpret_cast<const std::uint32_t*>(ctx.esp + 0x08);
					const auto expectedDestinationPitch = static_cast<std::uint32_t>(s_instance_->m_newResX) * bytesPerPixel;

					if (destinationPitch != expectedDestinationPitch)
					{
						return;
					}

					const auto offsetX = (static_cast<std::uint32_t>(s_instance_->m_newResX) - nativeVideoWidth) / 2;
					const auto offsetY = (static_cast<std::uint32_t>(s_instance_->m_newResY) - nativeVideoHeight) / 2;
					const auto destinationByteOffset = offsetY * destinationPitch + offsetX * bytesPerPixel;
					*destinationArgument += destinationByteOffset;
				});
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

			auto CameraFOVScansResult = Memory::PatternScan(m_fnxDX7Module, "8B 8E ?? ?? ?? ?? 52", "D9 87 ?? ?? ?? ?? 8D 4C 24");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)m_fnxDX7Module);
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)m_fnxDX7Module);

				Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

				m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.esi + 0xCC);

					if (fCurrentCameraFOV1 != 1.134464025497f)
					{
						s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
					}

					ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV1);
				});

				Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

				m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.edi + 0xCC);

					if (fCurrentCameraFOV2 != 1.134464025497f)
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale);
					}

					FPU::FLD(s_instance_->m_newCameraFOV2);
				});
			}

			if (m_skipIntroVideos == true)
			{
				auto SkipIntroVideosScansResult = Memory::PatternScan(ExeModule(), "0F 84 ?? ?? ?? ?? 6A ?? E8 ?? ?? ?? ?? 83 C4 ?? 85 C0",
				"E8 ?? ?? ?? ?? 8B 86 ?? ?? ?? ?? 89 9E");
				if (Memory::AreAllSignaturesValid(SkipIntroVideosScansResult) == true)
				{
					spdlog::info("Cryo Logo Intro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[CryoLogo] - (std::uint8_t*)ExeModule());
					spdlog::info("In Utero Intro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), SkipIntroVideosScansResult[InUteroLogo] - (std::uint8_t*)ExeModule());

					Memory::PatchBytes(SkipIntroVideosScansResult[CryoLogo], "\xE9\xA8\x00\x00\x00\x90");
					Memory::PatchBytes(SkipIntroVideosScansResult[InUteroLogo], "\x83\xC4\x14\xB0\x01");
				}
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	HMODULE m_fnxDX7Module = nullptr;
	HMODULE m_fnxCoreModule = nullptr;
	HMODULE m_4xmSdkDllModule = nullptr;
	std::string m_fnxDX7ModuleName = "";
	std::string m_fnxCoreModuleName = "";
	std::string m_4xmSdkDllModuleName = "";

	bool m_skipIntroVideos = false;

	std::vector<std::uint8_t*> ResolutionScans2Result;

	int m_newDefaultWidth = 0;
	int m_newDefaultHeight = 0;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_fmvsResolutionHook{};
	SafetyHookMid m_fmvsScaleHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum ResolutionInstructions1Index
	{
		ListUnlock,
		DefaultRes
	};

	enum ResolutionInstructions2Index
	{
		WidthHeight,
		FMVs,
		FMVsScale
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2
	};

	enum SkipIntroVideosInstructionsIndex
	{
		CryoLogo,
		InUteroLogo
	};

	inline static JekyllFix* s_instance_ = nullptr;
};

static std::unique_ptr<JekyllFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<JekyllFix>(hModule);
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