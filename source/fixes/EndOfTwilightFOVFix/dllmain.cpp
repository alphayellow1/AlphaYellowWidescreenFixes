#include "..\..\common\FixBase.hpp"
#include <regex>

class EndOfTwilightFix final : public FixBase
{
public:
	explicit EndOfTwilightFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~EndOfTwilightFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EndOfTwilightFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "End of Twilight";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "mol.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		Init();

		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		m_dllModule1 = Memory::GetHandle("e3d.dll", 50);
		m_dllModule2 = Memory::GetHandle("mol-ui.dll", 50);
		m_dllModule3 = Memory::GetHandle("ui.dll", 50);
		m_dllModule4 = Memory::GetHandle("mol-gameflow.dll", 50);
		m_dllModule5 = Memory::GetHandle("mol-script.dll", 50);
		m_dllModule1Name = Memory::GetModuleName(m_dllModule1);
		m_dllModule2Name = Memory::GetModuleName(m_dllModule2);
		m_dllModule3Name = Memory::GetModuleName(m_dllModule3);
		m_dllModule4Name = Memory::GetModuleName(m_dllModule4);
		m_dllModule5Name = Memory::GetModuleName(m_dllModule5);

		auto AspectRatioScansResult = Memory::PatternScan(m_dllModule2, "C7 46 ?? ?? ?? ?? ?? D9 9F", m_dllModule3, "C7 45 ?? ?? ?? ?? ?? 89 9E",
		m_dllModule4, "C7 40 ?? ?? ?? ?? ?? EB ?? 33 C0", m_dllModule5, "C7 40 ?? ?? ?? ?? ?? EB ?? 33 C0");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("UI Aspect Ratio Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), AspectRatioScansResult[UI_AR] - (std::uint8_t*)m_dllModule2);
			spdlog::info("Main Menu Background Aspect Ratio Instruction: Address is {:s}+{:x}", m_dllModule3Name.c_str(), AspectRatioScansResult[MAINMENU_AR] - (std::uint8_t*)m_dllModule3);
			spdlog::info("Gameplay Aspect Ratio Instruction: Address is {:s}+{:x}", m_dllModule4Name.c_str(), AspectRatioScansResult[GAMEPLAY_AR] - (std::uint8_t*)m_dllModule4);
			spdlog::info("Mol Script Aspect Ratio Instruction: Address is{:s} + {:x}", m_dllModule5Name.c_str(), AspectRatioScansResult[MOLSCRIPT_AR] - (std::uint8_t*)m_dllModule5);

			Memory::Write(AspectRatioScansResult, UI_AR, MOLSCRIPT_AR, 3, m_newAspectRatio);
		}

		auto CameraFOVScansResult = Memory::PatternScan(m_dllModule2, "C7 85 ?? ?? ?? ?? ?? ?? ?? ?? C7 45", m_dllModule3, "C7 87 ?? ?? ?? ?? ?? ?? ?? ?? C7 07",
		m_dllModule1, "C7 83 ?? ?? ?? ?? ?? ?? ?? ?? C7 03", m_dllModule5, "C7 83 ?? ?? ?? ?? ?? ?? ?? ?? C7 03");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("UI Camera FOV Instruction: Address is {:s}+{:x}", m_dllModule2Name.c_str(), CameraFOVScansResult[UI_FOV] - (std::uint8_t*)m_dllModule2);
			spdlog::info("Main Menu Background Camera FOV Instruction: Address is {:s}+{:x}", m_dllModule3Name.c_str(), CameraFOVScansResult[MAINMENU_FOV] - (std::uint8_t*)m_dllModule3);
			spdlog::info("Gameplay Camera FOV Instruction: Address is {:s}+{:x}", m_dllModule1Name.c_str(), CameraFOVScansResult[GAMEPLAY_FOV] - (std::uint8_t*)m_dllModule1);
			spdlog::info("Mol Script Camera FOV Instruction: Address is{:s} + {:x}", m_dllModule5Name.c_str(), CameraFOVScansResult[MOLSCRIPT_FOV] - (std::uint8_t*)m_dllModule5);

			m_newCameraFOV = Maths::CalculateNewFOV_RadBased(m_originalCameraFOV, m_aspectRatioScale);

			Memory::Write(CameraFOVScansResult[UI_FOV] + 6, m_newCameraFOV);
			Memory::Write(CameraFOVScansResult[MAINMENU_FOV] + 6, m_newCameraFOV);
			Memory::Write(CameraFOVScansResult[GAMEPLAY_FOV] + 6, m_newCameraFOV * m_fovFactor);
			Memory::Write(CameraFOVScansResult[MOLSCRIPT_FOV] + 6, m_newCameraFOV);
		}
	}

private:
	HMODULE m_dllModule1 = nullptr;
	HMODULE m_dllModule2 = nullptr;
	HMODULE m_dllModule3 = nullptr;
	HMODULE m_dllModule4 = nullptr;
	HMODULE m_dllModule5 = nullptr;
	std::string m_dllModule1Name = "";
	std::string m_dllModule2Name = "";
	std::string m_dllModule3Name = "";
	std::string m_dllModule4Name = "";
	std::string m_dllModule5Name = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 1.222000003f;

	enum AspectRatioInstructionsIndex
	{
		UI_AR,
		MAINMENU_AR,
		GAMEPLAY_AR,
		MOLSCRIPT_AR
	};

	enum CameraFOVInstructionsIndex
	{
		UI_FOV,
		MAINMENU_FOV,
		GAMEPLAY_FOV,
		MOLSCRIPT_FOV,
	};

	void Init()
	{
		if (!InitMolResolution())
		{
			spdlog::warn("Could not initialize resolution from registry, using fallback");

			m_newResX = 800;
			m_newResY = 600;
		}
	}

	bool ReadMolVideoMode(std::string& outVideoMode)
	{
		char buffer[256]{};
		DWORD bufferSize = sizeof(buffer);

		LSTATUS status = RegGetValueA(HKEY_CURRENT_USER, "SOFTWARE\\mol", "videoMode", RRF_RT_REG_SZ, nullptr, buffer, &bufferSize);

		if (status != ERROR_SUCCESS)
		{
			spdlog::error("Failed to read HKCU\\SOFTWARE\\mol\\videoMode. RegGetValueA status: {}", status);
			return false;
		}

		outVideoMode = buffer;
		return true;
	}

	bool ParseVideoModeResolution(const std::string& videoMode, int& outWidth, int& outHeight)
	{
		static const std::regex pattern(R"((\d+)\s*[xX]\s*(\d+)\s*[xX]\s*(\d+)\s*$)");

		std::smatch match;

		if (!std::regex_search(videoMode, match, pattern))
		{
			spdlog::error("Failed to parse resolution from videoMode string: '{}'", videoMode);
			return false;
		}

		try
		{
			const int width = std::stoi(match[1].str());
			const int height = std::stoi(match[2].str());
			const int bpp = std::stoi(match[3].str());

			if (width <= 0 || height <= 0)
			{
				spdlog::error("Invalid parsed resolution from videoMode string: '{}'", videoMode);
				return false;
			}

			outWidth = width;
			outHeight = height;

			return true;
		}
		catch (const std::exception& e)
		{
			spdlog::error("Exception while parsing videoMode string '{}': {}", videoMode, e.what());
			return false;
		}
	}

	bool InitMolResolution()
	{
		std::string videoMode;

		if (!ReadMolVideoMode(videoMode))
		{
			return false;
		}

		if (!ParseVideoModeResolution(videoMode, m_newResX, m_newResY))
		{
			return false;
		}

		return true;
	}

	inline static EndOfTwilightFix* s_instance_ = nullptr;
};

static std::unique_ptr<EndOfTwilightFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<EndOfTwilightFix>(hModule);
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