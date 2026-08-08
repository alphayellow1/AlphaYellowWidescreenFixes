#include "..\..\common\FixBase.hpp"

class MelbourneCupChallengeFix final : public FixBase
{
public:
	explicit MelbourneCupChallengeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~MelbourneCupChallengeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "MelbourneCupChallengeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Melbourne Cup Challenge";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Racing.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		m_resolutionScanResult = Memory::PatternScan(ExeModule(), "8B 0F 89 0D ?? ?? ?? ?? 8B 57");
		if (m_resolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(m_resolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.edi);
				s_instance_->m_newResY = Memory::ReadMem(ctx.edi + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		m_aspectRatioScansResult = Memory::PatternScan(ExeModule(), "E8 ?? ?? ?? ?? D9 44 24 ?? D8 0D ?? ?? ?? ?? D8 46", "8B 4F ?? 50 51 8B CE");
		if (Memory::AreAllSignaturesValid(m_aspectRatioScansResult) == true)
		{
			spdlog::info("Camera Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScansResult[Camera] - (std::uint8_t*)ExeModule());
			spdlog::info("HUD Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScansResult[HUD] - (std::uint8_t*)ExeModule());

			m_cameraAspectRatioHook = safetyhook::create_mid(m_aspectRatioScansResult[Camera], [](SafetyHookContext& ctx)
			{
				auto* arguments = reinterpret_cast<float*>(ctx.esp);

				float& horizontalExtent = arguments[0];
				float& verticalExtent = arguments[1];

				horizontalExtent = verticalExtent * s_instance_->m_newAspectRatio;
			});

			Memory::WriteNOPs(m_aspectRatioScansResult[HUD], 3);

			m_hudAspectRatioHook = safetyhook::create_mid(m_aspectRatioScansResult[HUD], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentHUDAspectRatio = Memory::ReadMem(ctx.edi + 0x30);
				s_instance_->m_newHUDAspectRatio = s_instance_->m_currentHUDAspectRatio * s_instance_->m_aspectRatioScale;
				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newHUDAspectRatio);
			});
		}

		m_cameraFOVScanResult = Memory::PatternScan(ExeModule(), "8B 4C 24 10 50 E8 ?? ?? ?? ?? 83 7C 24 30 10 C6 84 24 0C 01 00 00 05");
		if (m_cameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScanResult - (std::uint8_t*)ExeModule());
			
			m_cameraFOVHook = safetyhook::create_mid(m_cameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				auto* framing = reinterpret_cast<ManualFramingDefinition*>(ctx.eax);
				const auto* shot = reinterpret_cast<const ShotDefinition*>(ctx.ebp);

				if (framing == nullptr || shot == nullptr || shot->name == nullptr)
				{
					return;
				}

				if (std::strcmp(shot->name, "shakyfollow") == 0)
				{
					s_instance_->m_newShakyFollowViewWidth = 0.5f * s_instance_->m_fovFactor;
					s_instance_->m_newShakyFollowViewHeight = 0.375f * s_instance_->m_fovFactor;

					framing->viewWidth = s_instance_->m_newShakyFollowViewWidth;
					framing->viewHeight = s_instance_->m_newShakyFollowViewHeight;
				}				
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			m_multipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "74 ?? A1 ?? ?? ?? ?? 50 FF 15 ?? ?? ?? ?? 32 C0");
			if (m_multipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instances Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_multipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_multipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	bool m_runMultipleInstances = false;

	std::uint8_t* m_resolutionScanResult = nullptr;
	std::vector<std::uint8_t*> m_aspectRatioScansResult{};
	std::uint8_t* m_cameraFOVScanResult = nullptr;
	std::uint8_t* m_multipleInstancesCheckScanResult = nullptr;

	float m_currentHUDAspectRatio = 0.0f;
	float m_newHUDAspectRatio = 0.0f;

	float m_newShakyFollowViewWidth = 0.0f;
	float m_newShakyFollowViewHeight = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraAspectRatioHook{};
	SafetyHookMid m_hudAspectRatioHook{};
	SafetyHookMid m_cameraFOVHook{};

	struct ManualFramingDefinition
	{
		std::byte unknown00[0x18];
		std::int32_t viewMode;
		float viewWidth;
		float viewHeight;
		std::int32_t nearClipMode;
		float nearClip;
		float maxHorizontalFov;
		float maxHorizontalTan;
		float lookFactor;
	};

	struct ShotDefinition
	{
		const char* name;
	};

	enum AspectRatioInstructionsIndex
	{
		Camera,
		HUD
	};

	inline static MelbourneCupChallengeFix* s_instance_ = nullptr;
};

static std::unique_ptr<MelbourneCupChallengeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<MelbourneCupChallengeFix>(hModule);
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