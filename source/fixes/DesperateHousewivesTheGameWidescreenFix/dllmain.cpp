#include "..\..\common\FixBase.hpp"

class DesperateHousewivesFix final : public FixBase
{
public:
	explicit DesperateHousewivesFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DesperateHousewivesFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DesperateHousewivesTheGameWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Desperate Housewives: The Game";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "DesperateHousewives.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionListsScansResult = Memory::PatternScan(ExeModule(), "C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B FF 04",
		"C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24 ?? 8B 4C 24 ?? C7 00 ?? ?? ?? ?? C7 01 ?? ?? ?? ?? C3 8B 54 24 ?? 8B 44 24 ?? C7 02 ?? ?? ?? ?? C7 00 ?? ?? ?? ?? C3 8B 4C 24 ?? 8B 54 24 ?? C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B FF 94");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[List1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListsScansResult[List2] - (std::uint8_t*)ExeModule());

			// Resolution List 1
			// 640x480
			Memory::Write(ResolutionListsScansResult[List1] + 2, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 8, m_newResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[List1] + 23, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 29, m_newResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[List1] + 44, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 50, m_newResY);

			// 1152x864
			Memory::Write(ResolutionListsScansResult[List1] + 65, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 71, m_newResY);

			// 1290x960
			Memory::Write(ResolutionListsScansResult[List1] + 86, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 92, m_newResY);

			// 1280x1024
			Memory::Write(ResolutionListsScansResult[List1] + 107, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 113, m_newResY);

			// 1600x1200
			Memory::Write(ResolutionListsScansResult[List1] + 128, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 134, m_newResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[List1] + 149, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 155, m_newResY);

			// 960x720
			Memory::Write(ResolutionListsScansResult[List1] + 170, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 176, m_newResY);

			// 1200x900
			Memory::Write(ResolutionListsScansResult[List1] + 191, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 197, m_newResY);

			// 1067x800
			Memory::Write(ResolutionListsScansResult[List1] + 212, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 218, m_newResY);

			// Resolution List 2
			// 640x480
			Memory::Write(ResolutionListsScansResult[List2] + 2, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 8, m_newResY);

			// 800x600
			Memory::Write(ResolutionListsScansResult[List2] + 23, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 29, m_newResY);

			// 1024x768
			Memory::Write(ResolutionListsScansResult[List2] + 44, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 50, m_newResY);

			// 1152x864
			Memory::Write(ResolutionListsScansResult[List2] + 65, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 71, m_newResY);

			// 1290x960
			Memory::Write(ResolutionListsScansResult[List2] + 86, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 92, m_newResY);

			// 1280x1024
			Memory::Write(ResolutionListsScansResult[List2] + 107, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 113, m_newResY);

			// 1600x1200
			Memory::Write(ResolutionListsScansResult[List2] + 128, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 134, m_newResY);

			// 1280x768
			Memory::Write(ResolutionListsScansResult[List2] + 149, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 155, m_newResY);

			// 1280x720
			Memory::Write(ResolutionListsScansResult[List2] + 170, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 176, m_newResY);

			// 1600x900
			Memory::Write(ResolutionListsScansResult[List2] + 191, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 197, m_newResY);

			// 1280x800
			Memory::Write(ResolutionListsScansResult[List2] + 212, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 218, m_newResY);
		}

		auto FamilySelectionAspectRatioInstructionScanResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D9 58 ?? D9 E8 D9 58 ?? D9 05");
		if (FamilySelectionAspectRatioInstructionScanResult)
		{
			spdlog::info("Family Selection Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), FamilySelectionAspectRatioInstructionScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(FamilySelectionAspectRatioInstructionScanResult, 6);

			m_familySelectionAspectRatioHook = safetyhook::create_mid(FamilySelectionAspectRatioInstructionScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FLD(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate family selection aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "D9 03 D9 1C ?? 50", "A3 ?? ?? ?? ?? D9 05");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 2);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.ebx);
				s_instance_->m_newCameraFOV1 = fCurrentCameraFOV1 * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV1);
			});

			m_cameraFOV2ValueAddress = Memory::GetPointerFromAddress(CameraFOVInstructionsScansResult[FOV2] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 5);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				const float& fCurrentCameraFOV2 = Memory::ReadRegister(ctx.eax);
				s_instance_->m_newCameraFOV2 = fCurrentCameraFOV2 * s_instance_->m_fovFactor;
				*reinterpret_cast<float*>(s_instance_->m_cameraFOV2ValueAddress) = s_instance_->m_newCameraFOV2;
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;
	uintptr_t m_cameraFOV2ValueAddress = 0;

	enum ResolutionListsIndices
	{
		List1,
		List2
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	SafetyHookMid m_familySelectionAspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	inline static DesperateHousewivesFix* s_instance_ = nullptr;
};

static std::unique_ptr<DesperateHousewivesFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<DesperateHousewivesFix>(hModule);
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