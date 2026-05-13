#include "..\..\common\FixBase.hpp"

class AirportTycoon3Fix final : public FixBase
{
public:
	explicit AirportTycoon3Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AirportTycoon3Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AirportTycoon3WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Airport Tycoon 3";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "at3.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		InstallResolutionHooks();

		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8b 02 a3 ?? ?? ?? ?? 8b 42");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.edx);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);

				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 46 ?? 8D 44 24 ?? D8 0D");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 3);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid g_videoModeHook{};
	SafetyHookMid g_videoModeResultHook{};
	SafetyHookMid g_videoModeEnumLogHook{};
	SafetyHookMid g_videoModeFallbackResultHook{};
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	std::uintptr_t g_base = 0;

	static constexpr std::uintptr_t OFF_HOOK_VIDEO_MODE_CALL = 0xBE2D0;
	static constexpr std::uintptr_t OFF_HOOK_VIDEO_MODE_RESULT = 0xBE2DA;
	static constexpr std::uintptr_t OFF_HOOK_VIDEO_MODE_ENUM_LOG = 0xBE1C2;
	static constexpr std::uintptr_t OFF_HOOK_VIDEO_MODE_FALLBACK_RESULT = 0xBE2F7;

	static constexpr std::uintptr_t OFF_WIDTH = 0x3DC7E4;
	static constexpr std::uintptr_t OFF_HEIGHT = 0x3DC7E8;
	static constexpr std::uintptr_t OFF_COLORDEPTH = 0x3DC7EC;

	LONG g_config_mode_index = -1;
	LONG g_video_mode_index_logged = 0;

	int g_config_width = 0;
	int g_config_height = 0;
	int g_config_colordepth = 32;

	LONG g_video_mode_enum_log_count = 0;
	LONG g_video_mode_result_logged = 0;
	LONG g_video_mode_fallback_logged = 0;

	bool ParseResolution(const char* text, int& out_width, int& out_height)
	{
		if (!text)
		{
			return false;
		}

		while (*text == ' ' || *text == '\t') {
			++text;
		}

		char* end = nullptr;

		long width = std::strtol(text, &end, 10);
		if (end == text || width <= 0)
		{
			return false;
		}

		while (*end == ' ' || *end == '\t')
		{
			++end;
		}

		if (*end != 'x' && *end != 'X')
		{
			return false;
		}

		++end;

		while (*end == ' ' || *end == '\t')
		{
			++end;
		}

		char* height_end = nullptr;
		long height = std::strtol(end, &height_end, 10);
		if (height_end == end || height <= 0)
		{
			return false;
		}

		out_width = static_cast<int>(width);
		out_height = static_cast<int>(height);
		return true;
	}

	bool LoadGameConfigResolution()
	{
		const auto configPath = ExePath() / "data" / "config.txt";

		std::ifstream configFile(configPath);
		if (!configFile)
		{
			spdlog::error("Could not open game config file: {}", configPath.string());
			return false;
		}

		inipp::Ini<char> gameIni;
		gameIni.parse(configFile);
		gameIni.strip_trailing_comments();

		std::string resolutionString;
		int colorDepth = 32;

		if (!inipp::get_value(gameIni.sections["Init"], "Resolution", resolutionString))
		{
			spdlog::error("Could not find Resolution entry in {}", configPath.string());
			return false;
		}

		if (!ParseResolution(resolutionString.c_str(), g_config_width, g_config_height))
		{
			spdlog::error("Failed to parse Resolution value: {}", resolutionString);
			return false;
		}

		if (inipp::get_value(gameIni.sections["Init"], "Colordepth", colorDepth))
		{
			if (colorDepth == 16 || colorDepth == 32)
			{
				g_config_colordepth = colorDepth;
			}
			else
			{
				g_config_colordepth = 32;
			}
		}
		else
		{
			g_config_colordepth = 32;
		}

		return true;
	}

	LONG g_video_mode_hook_hit = 0;

	void VideoModeCallMidHook(SafetyHookContext& ctx)
	{
		if (g_config_width <= 0 || g_config_height <= 0)
		{
			return;
		}

		const std::uint32_t width = static_cast<std::uint32_t>(g_config_width);
		const std::uint32_t height = static_cast<std::uint32_t>(g_config_height);
		const std::uint32_t depth = static_cast<std::uint32_t>((g_config_colordepth == 16 || g_config_colordepth == 32) ? g_config_colordepth : 32);

		*reinterpret_cast<std::uint32_t*>(g_base + OFF_WIDTH) = width;
		*reinterpret_cast<std::uint32_t*>(g_base + OFF_HEIGHT) = height;
		*reinterpret_cast<std::uint32_t*>(g_base + OFF_COLORDEPTH) = depth;

		ctx.eax = depth;
		ctx.ecx = height;
		ctx.ebx = width;
	}

	void VideoModeResultMidHook(SafetyHookContext& ctx)
	{
		const std::uint32_t original_result = ctx.eax;

		if (ctx.eax == 0 && g_config_mode_index > 0)
		{
			ctx.eax = static_cast<std::uint32_t>(g_config_mode_index);
		}
	}

	LONG g_video_mode_fallback_result_logged = 0;

	void VideoModeFallbackResultMidHook(SafetyHookContext& ctx)
	{
		const std::uint32_t original_result = ctx.eax;

		if (g_config_mode_index > 0)
		{
			ctx.eax = static_cast<std::uint32_t>(g_config_mode_index);

			*reinterpret_cast<std::uint32_t*>(g_base + OFF_WIDTH) = static_cast<std::uint32_t>(g_config_width);

			*reinterpret_cast<std::uint32_t*>(g_base + OFF_HEIGHT) = static_cast<std::uint32_t>(g_config_height);

			*reinterpret_cast<std::uint32_t*>(g_base + OFF_COLORDEPTH) = static_cast<std::uint32_t>(g_config_colordepth);
		}
	}

	void VideoModeFallbackCallMidHook(SafetyHookContext& ctx)
	{
		if (g_config_width <= 0 || g_config_height <= 0)
		{
			return;
		}

		const std::uint32_t width = static_cast<std::uint32_t>(g_config_width);
		const std::uint32_t height = static_cast<std::uint32_t>(g_config_height);
		const std::uint32_t depth = static_cast<std::uint32_t>((g_config_colordepth == 16 || g_config_colordepth == 32) ? g_config_colordepth : 32);

		*reinterpret_cast<std::uint32_t*>(ctx.esp) = height;
		*reinterpret_cast<std::uint32_t*>(ctx.esp + 4) = depth;
		ctx.ebx = width;

		*reinterpret_cast<std::uint32_t*>(g_base + OFF_WIDTH) = width;
		*reinterpret_cast<std::uint32_t*>(g_base + OFF_HEIGHT) = height;
		*reinterpret_cast<std::uint32_t*>(g_base + OFF_COLORDEPTH) = depth;
	}

	void VideoModeEnumLogMidHook(SafetyHookContext& ctx)
	{
		const std::uint32_t mode_width = *reinterpret_cast<std::uint32_t*>(ctx.esp + 0x14);

		const std::uint32_t mode_height = *reinterpret_cast<std::uint32_t*>(ctx.esp + 0x18);

		const std::uint32_t mode_depth = *reinterpret_cast<std::uint32_t*>(ctx.esp + 0x1C);

		const std::uint32_t target_depth = static_cast<std::uint32_t>((g_config_colordepth == 16 || g_config_colordepth == 32) ? g_config_colordepth : 32);

		if (mode_width == static_cast<std::uint32_t>(g_config_width) && mode_height == static_cast<std::uint32_t>(g_config_height) && mode_depth == target_depth)
		{
			const LONG found_index = static_cast<LONG>(ctx.esi);

			if (found_index > 0)
			{
				const LONG previous = InterlockedCompareExchange(&g_config_mode_index, found_index, -1);
			}
		}

		const LONG count = InterlockedIncrement(&g_video_mode_enum_log_count);
	}

	bool g_resolution_hooks_installed = false;

	void InstallResolutionHooks()
	{
		if (g_resolution_hooks_installed)
		{
			return;
		}

		g_base = reinterpret_cast<std::uintptr_t>(ExeModule());

		if (LoadGameConfigResolution() == false)
		{
			spdlog::error("Resolution hook not installed because data/config.txt could not be parsed.");
			return;
		}

		g_config_mode_index = -1;

		*reinterpret_cast<std::uint32_t*>(g_base + OFF_WIDTH) = static_cast<std::uint32_t>(g_config_width);

		*reinterpret_cast<std::uint32_t*>(g_base + OFF_HEIGHT) = static_cast<std::uint32_t>(g_config_height);

		*reinterpret_cast<std::uint32_t*>(g_base + OFF_COLORDEPTH) = static_cast<std::uint32_t>(g_config_colordepth);

		g_videoModeHook = safetyhook::create_mid(reinterpret_cast<void*>(g_base + OFF_HOOK_VIDEO_MODE_CALL), [](SafetyHookContext& ctx)
		{
			s_instance_->VideoModeCallMidHook(ctx);
		});

		g_videoModeResultHook = safetyhook::create_mid(reinterpret_cast<void*>(g_base + OFF_HOOK_VIDEO_MODE_RESULT), [](SafetyHookContext& ctx)
		{
			s_instance_->VideoModeResultMidHook(ctx);
		});

		g_videoModeEnumLogHook = safetyhook::create_mid(reinterpret_cast<void*>(g_base + OFF_HOOK_VIDEO_MODE_ENUM_LOG), [](SafetyHookContext& ctx)
		{
			s_instance_->VideoModeEnumLogMidHook(ctx);
		});

		g_videoModeFallbackResultHook = safetyhook::create_mid(reinterpret_cast<void*>(g_base + OFF_HOOK_VIDEO_MODE_FALLBACK_RESULT), [](SafetyHookContext& ctx)
			{
				s_instance_->VideoModeFallbackResultMidHook(ctx);
			});

		g_resolution_hooks_installed = true;
	}

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.esi + 0x40);
		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;
		FPU::FLD(m_newCameraFOV);
	}

	inline static AirportTycoon3Fix* s_instance_ = nullptr;
};

static std::unique_ptr<AirportTycoon3Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AirportTycoon3Fix>(hModule);
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