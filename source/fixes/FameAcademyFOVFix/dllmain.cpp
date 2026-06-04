#include "..\..\common\FixBase.hpp"

class FameAcademyFix final : public FixBase
{
public:
	explicit FameAcademyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~FameAcademyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "FameAcademyFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Fame Academy";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Game.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		GetResolutionFromIni();

		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);

		auto AspectRatioScansResult = Memory::PatternScan(ExeModule(), "8B 96 ?? ?? ?? ?? 50 8B 86 ?? ?? ?? ?? 51 52 50 8D 4E", "D8 8E ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C", "D8 8E ?? ?? ?? ?? 83 C4", "D8 8E ?? ?? ?? ?? D8 C9");
		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 86 ?? ?? ?? ?? 51 52 50 8D 4E", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 51", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D9 1C", "D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? D8 0D", "D9 86 ?? ?? ?? ?? D8 8E ?? ?? ?? ?? 83 C4", "D9 86 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 8D 5E");

		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(AspectRatioScansResult[AR1], 6);

			m_aspectRatio1Hook = safetyhook::create_mid(AspectRatioScansResult[AR1], [](SafetyHookContext& ctx)
			{
				float& fCurrentAspectRatio = Memory::ReadMem(ctx.esi + 0xBC);

				if (fCurrentAspectRatio == 1.333299994f)
				{
					s_instance_->m_newAspectRatio2 = s_instance_->m_newAspectRatio;
				}
				else
				{
					s_instance_->m_newAspectRatio2 = fCurrentAspectRatio;
				}

				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio2);
			});

			Memory::WriteNOPs(AspectRatioScansResult[AR2], 6);

			m_aspectRatio2Hook = safetyhook::create_mid(AspectRatioScansResult[AR2], AspectRatioMidHook);

			Memory::WriteNOPs(AspectRatioScansResult[AR3], 6);

			m_aspectRatio3Hook = safetyhook::create_mid(AspectRatioScansResult[AR3], AspectRatioMidHook);

			Memory::WriteNOPs(AspectRatioScansResult[AR4], 6);

			m_aspectRatio4Hook = safetyhook::create_mid(AspectRatioScansResult[AR4], AspectRatioMidHook);
		}

		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.esi + 0xB8);

				if (fCurrentCameraFOV2 == 0.7853981853f)
				{
					s_instance_->m_newCameraFOV2 = fCurrentCameraFOV2 * s_instance_->m_fovFactor;
				}
				else
				{
					s_instance_->m_newCameraFOV2 = fCurrentCameraFOV2;
				}

				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV2);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], CameraFOVMidHook);

			Memory::WriteNOPs(CameraFOVScansResult[FOV3], 6);

			m_cameraFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[FOV3], CameraFOVMidHook);

			Memory::WriteNOPs(CameraFOVScansResult[FOV4], 6);

			m_cameraFOV4Hook = safetyhook::create_mid(CameraFOVScansResult[FOV4], CameraFOVMidHook);

			Memory::WriteNOPs(CameraFOVScansResult[FOV5], 6);

			m_cameraFOV5Hook = safetyhook::create_mid(CameraFOVScansResult[FOV5], CameraFOVMidHook);

			Memory::WriteNOPs(CameraFOVScansResult[FOV6], 6);

			m_cameraFOV6Hook = safetyhook::create_mid(CameraFOVScansResult[FOV6], CameraFOVMidHook);
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newCameraFOV2 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatio1Hook{};
	SafetyHookMid m_aspectRatio2Hook{};
	SafetyHookMid m_aspectRatio3Hook{};
	SafetyHookMid m_aspectRatio4Hook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};
	SafetyHookMid m_cameraFOV3Hook{};
	SafetyHookMid m_cameraFOV4Hook{};
	SafetyHookMid m_cameraFOV5Hook{};
	SafetyHookMid m_cameraFOV6Hook{};

	enum AspectRatioInstructionsIndex
	{
		AR1,
		AR2,
		AR3,
		AR4
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6
	};

	void GetResolutionFromIni()
	{
		char exePath[MAX_PATH]{};

		if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0)
		{
			spdlog::error("Failed to get executable path.");
			return;
		}

		std::filesystem::path configPath = std::filesystem::path(exePath).parent_path() / "DATA" / "Config" / "UserConfig.ini";

		inipp::Ini<char> fameAcademyIni;
		std::ifstream iniFile(configPath);

		if (!iniFile.is_open())
		{
			spdlog::error("Failed to open UserConfig.ini at: {}", configPath.string());
			return;
		}

		fameAcademyIni.parse(iniFile);

		int iniWidth = 0;
		int iniHeight = 0;

		inipp::get_value(fameAcademyIni.sections["CUSTOM_DISPLAY"], "DISPRESOLUTIONWIDTH", iniWidth);
		inipp::get_value(fameAcademyIni.sections["CUSTOM_DISPLAY"], "DISPRESOLUTIONHEIGHT", iniHeight);

		if (iniWidth <= 0 || iniHeight <= 0)
		{
			spdlog::error("Invalid resolution read from UserConfig.ini: {}x{}", iniWidth, iniHeight);
			return;
		}

		m_newResX = iniWidth;
		m_newResY = iniHeight;
	}

	static void AspectRatioMidHook(SafetyHookContext& ctx)
	{
		FPU::FMUL(s_instance_->m_newAspectRatio);
	}	

	static void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.esi + 0xB8);

		s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;

		FPU::FLD(s_instance_->m_newCameraFOV);
	}

	inline static FameAcademyFix* s_instance_ = nullptr;
};

static std::unique_ptr<FameAcademyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<FameAcademyFix>(hModule);
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