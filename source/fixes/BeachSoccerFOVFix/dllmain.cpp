#include "..\..\common\FixBase.hpp"

class BeachSoccerFix final : public FixBase
{
public:
	explicit BeachSoccerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BeachSoccerFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BeachSoccerFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Beach Soccer";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "BeachSoccer.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? DC 0D ?? ?? ?? ?? 8B 44 24");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 4);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->GetARAndScale();

				float& fCurrentCameraHFOV = Memory::ReadMem(ctx.esp + 0x4);
				s_instance_->m_newCameraHFOV = Maths::CalculateNewHFOV_RadBased(fCurrentCameraHFOV, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
				FPU::FLD(s_instance_->m_newCameraHFOV);
			});

			Memory::WriteNOPs(CameraFOVScanResult + 36, 4);

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScanResult + 36, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraVFOV = Memory::ReadMem(ctx.esp + 0x8);
				s_instance_->m_newCameraVFOV = Maths::CalculateNewVFOV_RadBased(fCurrentCameraVFOV * s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
				FPU::FLD(s_instance_->m_newCameraVFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	int m_gameWidth = 0;
	int m_gameHeight = 0;

	static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
	{
		DWORD windowProcessId = 0;
		GetWindowThreadProcessId(hwnd, &windowProcessId);

		if (windowProcessId != GetCurrentProcessId())
		{
			return TRUE;
		}

		if (!IsWindowVisible(hwnd))
		{
			return TRUE;
		}

		if (GetWindow(hwnd, GW_OWNER) != nullptr)
		{
			return TRUE;
		}

		*reinterpret_cast<HWND*>(lParam) = hwnd;
		return FALSE;
	}

	HWND GetGameWindow() const
	{
		HWND hwnd = nullptr;
		EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&hwnd));
		return hwnd;
	}

	void GetARAndScale()
	{
		HWND gameWindow = nullptr;

		if (!gameWindow || !IsWindow(gameWindow))
		{
			gameWindow = GetGameWindow();
		}

		RECT clientRect{};
		if (GetClientRect(gameWindow, &clientRect))
		{
			const int width = clientRect.right - clientRect.left;
			const int height = clientRect.bottom - clientRect.top;

			if (width > 0 && height > 0)
			{
				if (width != m_gameWidth || height != m_gameHeight)
				{
					m_gameWidth = width;
					m_gameHeight = height;

					m_newAspectRatio = static_cast<float>(width) / static_cast<float>(height);

					m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
				}
			}
		}
	}

	float m_newCameraHFOV = 0.0f;
	float m_newCameraVFOV = 0.0f;

	inline static BeachSoccerFix* s_instance_ = nullptr;
};

static std::unique_ptr<BeachSoccerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BeachSoccerFix>(hModule);
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