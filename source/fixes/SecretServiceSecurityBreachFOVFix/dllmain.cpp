#include "..\..\common\FixBase.hpp"

class SecretServiceSecurityBreachFix final : public FixBase
{
public:
	explicit SecretServiceSecurityBreachFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~SecretServiceSecurityBreachFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "SecretServiceSecurityBreachFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Secret Service: Security Breach";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		if (!Util::stringcmp_caseless(exeName, "run.exe"))
		{
			return false;
		}

		std::string commandLine = GetCommandLineA();

		std::transform(commandLine.begin(), commandLine.end(), commandLine.begin(),
		[](unsigned char c)
		{
			return static_cast<char>(std::tolower(c));
		});

		if (commandLine.find("--server") != std::string::npos)
		{
			return false;
		}

		return true;
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		ss2DllModule = Memory::GetHandle("ss2.dll", 100, 20);
		ss2DllModuleName = Memory::GetModuleName(ss2DllModule);

		GetResolutionFromCfg();

		auto CameraFOVScansResult = Memory::PatternScan(ss2DllModule, "D9 45 ?? D8 0D ?? ?? ?? ?? F6 46", "D9 81 ?? ?? ?? ?? 83 EC ?? 8B CE");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ss2DllModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ss2DllModule);
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ss2DllModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ss2DllModule);

			Memory::WriteNOPs(CameraFOVScansResult[FOV1], 3);

			m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ebp + 0x4, Rad, s_instance_->m_aspectRatioScale, 1.0f);
			});

			Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

			m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx.ecx + 0x200, Deg, 1.0f, s_instance_->m_fovFactor);
			});
		}
	}

private:
	HMODULE ss2DllModule = nullptr;
	std::string ss2DllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	void GetResolutionFromCfg()
	{
		char modulePath[MAX_PATH]{};

		if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) == 0)
		{
			spdlog::warn("Failed to get executable path. Falling back to 4:3 aspect ratio.");
			m_aspectRatioScale = 1.0f;
			return;
		}

		std::filesystem::path exePath(modulePath);
		std::filesystem::path gameDir = exePath.parent_path();

		std::filesystem::path cfgPath = gameDir / "ss2" / "settings" / "autoclient.cfg";

		std::ifstream cfg(cfgPath);
		if (!cfg.is_open())
		{
			spdlog::warn("Failed to open {}. Falling back to 4:3 aspect ratio.", cfgPath.string());
			m_aspectRatioScale = 1.0f;
			return;
		}

		std::string line;

		while (std::getline(cfg, line))
		{
			if (const auto commentPos = line.find("//"); commentPos != std::string::npos)
			{
				line.erase(commentPos);
			}

			std::istringstream iss(line);

			std::string command;
			std::string value;

			if (!(iss >> command >> value))
			{
				continue;
			}

			if (Util::stringcmp_caseless(command, "res"))
			{
				const auto xPos = value.find('x');

				if (xPos == std::string::npos)
				{
					spdlog::warn("Invalid resolution format in {}: {}", cfgPath.string(), value);
					m_aspectRatioScale = 1.0f;
					return;
				}

				try
				{
					const int width = std::stoi(value.substr(0, xPos));
					const int height = std::stoi(value.substr(xPos + 1));

					if (width <= 0 || height <= 0)
					{
						spdlog::warn("Invalid resolution values in {}: {}x{}", cfgPath.string(), width, height);
						m_aspectRatioScale = 1.0f;
						return;
					}

					const float currentAspectRatio = static_cast<float>(width) / static_cast<float>(height);

					m_aspectRatioScale = currentAspectRatio / m_oldAspectRatio;

					return;
				}
				catch (const std::exception& exception)
				{
					spdlog::warn("Failed to parse resolution from {}: {}. Error: {}", cfgPath.string(), value, exception.what());

					m_aspectRatioScale = 1.0f;
					return;
				}
			}
		}

		spdlog::warn("Resolution entry not found in {}. Falling back to 4:3 aspect ratio.", cfgPath.string());
		m_aspectRatioScale = 1.0f;
	}

	enum AngleMode
	{
		Deg,
		Rad
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, AngleMode angleMode, float arScale, float fovFactor)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(FOVAddress);

		if (angleMode == Rad)
		{
			m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, arScale) * fovFactor;
		}
		else if (angleMode == Deg)
		{
			m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, arScale) * fovFactor;
		}
		
		FPU::FLD(m_newCameraFOV);
	}

	inline static SecretServiceSecurityBreachFix* s_instance_ = nullptr;
};

static std::unique_ptr<SecretServiceSecurityBreachFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<SecretServiceSecurityBreachFix>(hModule);
		g_fix->Start();
		break;
	}


	case DLL_PROCESS_DETACH:
	{
		if (lpReserved == nullptr)
		{
			if (g_fix)
			{
				g_fix->Shutdown();
				g_fix.reset();
			}
		}

		break;
	}


	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	default:
		break;
	}

	return TRUE;
}