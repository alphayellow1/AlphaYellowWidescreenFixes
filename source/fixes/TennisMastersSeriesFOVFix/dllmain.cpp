#include "..\..\common\FixBase.hpp"

class TennisMastersSeriesFix final : public FixBase
{
public:
	explicit TennisMastersSeriesFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TennisMastersSeriesFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TennisMastersSeriesFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Tennis Masters Series";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Tennis Masters Series.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 51 ?? 89 50 ?? 8B 51 ?? 89 50 ?? 8B 51 ?? 89 50 ?? 8B 51 ?? 89 50 ?? 8A 51 ?? 88 50 ?? 8A 51 ?? 88 50 ?? 8B 51");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				uint32_t& iCurrentWidth = Memory::ReadMem(ctx.ecx + 0xC);
				uint32_t& iCurrentHeight = Memory::ReadMem(ctx.ecx + 0x10);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 8D 44 8B", "D8 0D ?? ?? ?? ?? 8D 44 86",
		"D8 0D ?? ?? ?? ?? 8D 44 81", "43 ?? E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ??", "D8 0D ?? ?? ?? ?? 8B 91 ?? ?? ?? ?? D9 5C 24",
		"86 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ??",
		"CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ??",
		"C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 8B 8E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instructions Scan 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Camera1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instructions Scan 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Camera2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instructions Scan 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Camera3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instructions Scan 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Camera4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instructions Scan 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Camera5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instructions Scan 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Camera6] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instructions Scan 7: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Camera7] - (std::uint8_t*)ExeModule());
			spdlog::info("Player Selection FOV Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[PlayerSelection] - (std::uint8_t*)ExeModule());

			m_cameraHFOV1Address = Memory::GetPointerFromAddress(CameraFOVScansResult[Camera1] + 2, Memory::PointerMode::Absolute);
			m_cameraHFOV2Address = Memory::GetPointerFromAddress(CameraFOVScansResult[Camera1] + 21, Memory::PointerMode::Absolute);
			m_cameraVFOV1Address = Memory::GetPointerFromAddress(CameraFOVScansResult[Camera1] + 33, Memory::PointerMode::Absolute);
			m_cameraVFOV2Address = Memory::GetPointerFromAddress(CameraFOVScansResult[Camera1] + 45, Memory::PointerMode::Absolute);

			// Camera 1
			Memory::WriteNOPs(CameraFOVScansResult[Camera1], 6);
			m_camera1_HFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV1Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera1] + 19, 6);
			m_camera1_HFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera1] + 19, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV2Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera1] + 31, 6);
			m_camera1_VFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera1] + 31, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV1Address, 1.0f, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera1] + 43, 6);
			m_camera1_VFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera1] + 43, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV2Address, 1.0f, s_instance_->m_fovFactor);
			});

			// Camera 2
			Memory::WriteNOPs(CameraFOVScansResult[Camera2], 6);
			m_camera2_HFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV1Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera2] + 16, 6);
			m_camera2_HFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera2] + 16, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV2Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera2] + 28, 6);
			m_camera2_VFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera2] + 28, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV1Address, 1.0f, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera2] + 40, 6);
			m_camera2_VFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera2] + 40, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV2Address, 1.0f, s_instance_->m_fovFactor);
			});

			// Camera 3
			Memory::WriteNOPs(CameraFOVScansResult[Camera3], 6);
			m_camera3_HFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera3], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV1Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera3] + 16, 6);
			m_camera3_HFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera3] + 16, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV2Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera3] + 28, 6);
			m_camera3_VFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera3] + 28, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV1Address, 1.0f, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera3] + 40, 6);
			m_camera3_VFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera3] + 40, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV2Address, 1.0f, s_instance_->m_fovFactor);
			});

			// Camera 4
			Memory::WriteNOPs(CameraFOVScansResult[Camera4] + 13, 6);
			m_camera4_HFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera4] + 13, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV1Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera4] + 45, 6);
			m_camera4_HFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera4] + 45, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV2Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera4] + 61, 6);
			m_camera4_VFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera4] + 61, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV1Address, 1.0f, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera4] + 77, 6);
			m_camera4_VFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera4] + 77, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV2Address, 1.0f, s_instance_->m_fovFactor);
			});

			// Camera 5
			Memory::WriteNOPs(CameraFOVScansResult[Camera5], 6);
			m_camera5_HFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera5], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV1Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera5] + 18, 6);
			m_camera5_HFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera5] + 18, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV2Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera5] + 30, 6);
			m_camera5_VFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera5] + 30, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV1Address, 1.0f, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera5] + 40, 6);
			m_camera5_VFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera5] + 40, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV2Address, 1.0f, s_instance_->m_fovFactor);
			});

			// Camera 6
			Memory::WriteNOPs(CameraFOVScansResult[Camera6] + 30, 6);
			m_camera6_HFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera6] + 30, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV1Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera6] + 62, 6);
			m_camera6_HFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera6] + 62, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV2Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera6] + 78, 6);
			m_camera6_VFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera6] + 78, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV1Address, 1.0f, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera6] + 94, 6);
			m_camera6_VFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera6] + 94, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV2Address, 1.0f, s_instance_->m_fovFactor);
			});

			// Camera 7
			Memory::WriteNOPs(CameraFOVScansResult[Camera7] + 26, 6);
			m_camera7_HFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera7] + 26, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV1Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera7] + 58, 6);
			m_camera7_HFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera7] + 58, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOV2Address, s_instance_->m_aspectRatioScale, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera7] + 74, 6);
			m_camera7_VFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Camera7] + 74, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV1Address, 1.0f, s_instance_->m_fovFactor);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Camera7] + 90, 6);
			m_camera7_VFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Camera7] + 90, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOV2Address, 1.0f, s_instance_->m_fovFactor);
			});

			// Player Selection Camera
			Memory::WriteNOPs(CameraFOVScansResult[PlayerSelection], 20);
			m_playerSelectionCameraHook = safetyhook::create_mid(CameraFOVScansResult[PlayerSelection], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newPlayerSelectionHFOV1 = m_originalPlayerSelectionHFOV1 * s_instance_->m_aspectRatioScale;
				s_instance_->m_newPlayerSelectionHFOV2 = m_originalPlayerSelectionHFOV2 * s_instance_->m_aspectRatioScale;
				*reinterpret_cast<float*>(ctx.ecx + 0x144) = s_instance_->m_newPlayerSelectionHFOV1;
				*reinterpret_cast<float*>(ctx.ecx + 0x148) = s_instance_->m_newPlayerSelectionHFOV2;
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalPlayerSelectionHFOV1 = -0.55f;
	static constexpr float m_originalPlayerSelectionHFOV2 = 0.55f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_camera1_HFOV1Hook{};
	SafetyHookMid m_camera1_HFOV2Hook{};
	SafetyHookMid m_camera1_VFOV1Hook{};
	SafetyHookMid m_camera1_VFOV2Hook{};
	SafetyHookMid m_camera2_HFOV1Hook{};
	SafetyHookMid m_camera2_HFOV2Hook{};
	SafetyHookMid m_camera2_VFOV1Hook{};
	SafetyHookMid m_camera2_VFOV2Hook{};
	SafetyHookMid m_camera3_HFOV1Hook{};
	SafetyHookMid m_camera3_HFOV2Hook{};
	SafetyHookMid m_camera3_VFOV1Hook{};
	SafetyHookMid m_camera3_VFOV2Hook{};
	SafetyHookMid m_camera4_HFOV1Hook{};
	SafetyHookMid m_camera4_HFOV2Hook{};
	SafetyHookMid m_camera4_VFOV1Hook{};
	SafetyHookMid m_camera4_VFOV2Hook{};
	SafetyHookMid m_camera5_HFOV1Hook{};
	SafetyHookMid m_camera5_HFOV2Hook{};
	SafetyHookMid m_camera5_VFOV1Hook{};
	SafetyHookMid m_camera5_VFOV2Hook{};
	SafetyHookMid m_camera6_HFOV1Hook{};
	SafetyHookMid m_camera6_HFOV2Hook{};
	SafetyHookMid m_camera6_VFOV1Hook{};
	SafetyHookMid m_camera6_VFOV2Hook{};
	SafetyHookMid m_camera7_HFOV1Hook{};
	SafetyHookMid m_camera7_HFOV2Hook{};
	SafetyHookMid m_camera7_VFOV1Hook{};
	SafetyHookMid m_camera7_VFOV2Hook{};
	SafetyHookMid m_playerSelectionCameraHook{};

	uintptr_t m_cameraHFOV1Address = 0;
	uintptr_t m_cameraHFOV2Address = 0;
	uintptr_t m_cameraVFOV1Address = 0;
	uintptr_t m_cameraVFOV2Address = 0;

	float m_newPlayerSelectionHFOV1 = 0.0f;
	float m_newPlayerSelectionHFOV2 = 0.0f;

	enum CameraFOVInstructionsIndex
	{
		Camera1,
		Camera2,
		Camera3,
		Camera4,
		Camera5,
		Camera6,
		Camera7,
		PlayerSelection
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, float arScale, float fovFactor)
	{
		float& fCurrentCameraFOV = *reinterpret_cast<float*>(FOVAddress);

		m_newCameraFOV = fCurrentCameraFOV * arScale * fovFactor;

		FPU::FMUL(m_newCameraFOV);
	}

	inline static TennisMastersSeriesFix* s_instance_ = nullptr;
};

static std::unique_ptr<TennisMastersSeriesFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<TennisMastersSeriesFix>(hModule);
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