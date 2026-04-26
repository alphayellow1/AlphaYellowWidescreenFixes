#include "..\..\common\FixBase.hpp"

class WoSConvoyFix final : public FixBase
{
public:
	explicit WoSConvoyFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WoSConvoyFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "18WheelsOfSteelConvoyFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "18 Wheels of Steel: Convoy";
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

		auto CameraFOVScansResult = Memory::PatternScan(gameDll, "C7 81 ?? ?? ?? ?? ?? ?? ?? ?? 32 C0", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4E ?? 3B CB 0F 84",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? EB ?? 8B 4E", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? BF", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4E ?? 3B CB 89 BE",
		"68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4E ?? 3B CB 74", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4E 34 3B CB C7 86 14 01 00 00 04", "68 ?? ?? ?? ?? E8 ?? ?? ?? ?? C7 86",
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

			m_newCameraFOV = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV, m_aspectRatioScale) * m_fovFactor;

			Memory::Write(CameraFOVScansResult[FOV1] + 6, m_newCameraFOV);
			Memory::Write(CameraFOVScansResult, FOV2, FOV8, 1, m_newCameraFOV);
			Memory::Write(CameraFOVScansResult[FOV9] + 3, m_newCameraFOV);
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

	static constexpr float m_originalCameraFOV = 57.0f;

	inline static WoSConvoyFix* s_instance_ = nullptr;
};

static std::unique_ptr<WoSConvoyFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<WoSConvoyFix>(hModule);

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