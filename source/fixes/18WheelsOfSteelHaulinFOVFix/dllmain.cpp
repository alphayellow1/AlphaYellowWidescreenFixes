#include "..\..\common\FixBase.hpp"

class WoSHaulinFix final : public FixBase
{
public:
	explicit WoSHaulinFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WoSHaulinFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "18WheelsOfSteelHaulinFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.0";
	}

	const char* TargetName() const override
	{
		return "18 Wheels of Steel: Haulin'";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "prism3d.exe");
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

		gameDll = Memory::GetHandle("game.dll");
		gameDllName = Memory::GetModuleName(gameDll);

		auto CameraFOVScansResult = Memory::PatternScan(gameDll, "C7 44 24 ?? ?? ?? ?? ?? 8D 54 24 ?? 55", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 89 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 57",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A ?? ?? ?? ?? 8B 01 FF 50 ?? 8B 7E", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 89 ?? ?? ?? ?? 8B 11 FF 52 ?? 8B 56",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A A8 01 00 00 8B 01 FF 50 34 39 5E 58 0F 95 C1 6A 00 51 8B 4E 20 E8 ?? ?? ?? ?? 39 5E 58 8B CE 75 17 6A 00 E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A B4 01 00 00 6A 01 EB 14 6A 01 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 88 B4 01 00 00 6A 00 E8 ?? ?? ?? ?? 68 84",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A A8 01 00 00 8B 01 FF 50 34 39 5E 58 0F 95 C1 6A 00 51 8B 4E 20 E8 ?? ?? ?? ?? 39 5E 58 8B CE 75 17 6A 00 E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A B4 01 00 00 6A 01 EB 14 6A 01 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 88 B4 01 00 00 6A 00 E8 ?? ?? ?? ?? 68 85",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A A8 01 00 00 8B 01 FF 50 34 39 5E 58 0F 95 C1 6A 00 51 8B 4E 20 E8 ?? ?? ?? ?? 39 5E 58 8B CE 75 17 6A 00 E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A B4 01 00 00 6A 01 EB 14 6A 01 E8 ?? ?? ?? ?? A1 ?? ?? ?? ?? 8B 88 B4 01 00 00 6A 00 E8 ?? ?? ?? ?? 68 86",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A ?? ?? ?? ?? 8B 01 FF 50 ?? 39 5E ?? 0F 95 C1 6A ?? 51 8B 4E ?? E8 ?? ?? ?? ?? 39 5E ?? 8B CE 75 ?? 6A ?? E8 ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 8B 8A ?? ?? ?? ?? 6A ?? E8",
		"C7 40 ?? ?? ?? ?? ?? 88 48 ?? D9 50");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV7] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV8] - (std::uint8_t*)gameDll);
			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", gameDllName.c_str(), CameraFOVScansResult[FOV9] - (std::uint8_t*)gameDll);

			m_newCameraFOV1 = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV1, m_aspectRatioScale);

			m_newCameraFOV2 = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV2, m_aspectRatioScale) * m_fovFactor;

			Memory::Write(CameraFOVScansResult[FOV1] + 4, m_newCameraFOV1);
			Memory::Write(CameraFOVScansResult, FOV2, FOV8, 1, m_newCameraFOV2);
			Memory::Write(CameraFOVScansResult[FOV9] + 3, m_newCameraFOV2);
		}
	}

private:
	HMODULE gameDll = nullptr;

	std::string gameDllName;

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2,
		FOV3,
		FOV4,
		FOV5,
		FOV6,
		FOV7,
		FOV8,
		FOV9
	};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	static constexpr float m_originalCameraFOV1 = 50.0f;
	static constexpr float m_originalCameraFOV2 = 57.0f;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	inline static WoSHaulinFix* s_instance_ = nullptr;
};

static std::unique_ptr<WoSHaulinFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<WoSHaulinFix>(hModule);

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