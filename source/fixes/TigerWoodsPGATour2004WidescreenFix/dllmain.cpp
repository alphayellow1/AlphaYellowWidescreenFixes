#include "..\..\common\FixBase.hpp"

class TigerWoodsPGATour2004Fix final : public FixBase
{
public:
	explicit TigerWoodsPGATour2004Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TigerWoodsPGATour2004Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TigerWoodsPGATour2004WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Tiger Woods: PGA Tour 2004";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "TW2004.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		m_dxWrapDllModule = Memory::GetHandle("DXWrap.dll");
		m_dxWrapDllModuleName = Memory::GetModuleName(m_dxWrapDllModule);		

		// Lists are located in the following functions in DXWrap.dll: FormatToBPP, CDisplay::FindMode, CDisplay::IsModeAvailable, CDisplay::RestoreSelectedMode, CDisplay::ForceToGameBitDepth
		m_resolutionScansResult = Memory::PatternScan(m_dxWrapDllModule, "C7 01 ?? ?? ?? ?? C7 02 ?? ?? ?? ?? C3 8B 44 24",
		"BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? 8B 44 24",
		"BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? BA ?? ?? ?? ?? B8 ?? ?? ?? ?? EB ?? 8B 84 24",
		"BF ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? BF ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? BF ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? BF ?? ?? ?? ?? BE ?? ?? ?? ?? EB ?? 8B 74 24",
		"BD ?? ?? ?? ?? BB ?? ?? ?? ?? EB ?? BD ?? ?? ?? ?? BB ?? ?? ?? ?? EB ?? BD ?? ?? ?? ?? BB ?? ?? ?? ?? EB ?? BD ?? ?? ?? ?? BB ?? ?? ?? ?? EB ?? 8B 5C 24");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", m_dxWrapDllModuleName.c_str(), m_resolutionScansResult[List1] - (std::uint8_t*)m_dxWrapDllModule);
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", m_dxWrapDllModuleName.c_str(), m_resolutionScansResult[List2] - (std::uint8_t*)m_dxWrapDllModule);
			spdlog::info("Resolution List 3 Scan: Address is {:s}+{:x}", m_dxWrapDllModuleName.c_str(), m_resolutionScansResult[List3] - (std::uint8_t*)m_dxWrapDllModule);
			spdlog::info("Resolution List 4 Scan: Address is {:s}+{:x}", m_dxWrapDllModuleName.c_str(), m_resolutionScansResult[List4] - (std::uint8_t*)m_dxWrapDllModule);
			spdlog::info("Resolution List 5 Scan: Address is {:s}+{:x}", m_dxWrapDllModuleName.c_str(), m_resolutionScansResult[List5] - (std::uint8_t*)m_dxWrapDllModule);

			// Resolution List 1
			// 800x600
			Memory::Write(m_resolutionScansResult[List1] + 2, m_newResX);
			Memory::Write(m_resolutionScansResult[List1] + 8, m_newResY);
			// 1024x768
			Memory::Write(m_resolutionScansResult[List1] + 23, m_newResX);
			Memory::Write(m_resolutionScansResult[List1] + 29, m_newResY);
			// 1280x1024
			Memory::Write(m_resolutionScansResult[List1] + 44, m_newResX);
			Memory::Write(m_resolutionScansResult[List1] + 50, m_newResY);
			// 1600x1200
			Memory::Write(m_resolutionScansResult[List1] + 65, m_newResX);
			Memory::Write(m_resolutionScansResult[List1] + 71, m_newResY);

			// Resolution List 2
			// 800x600
			Memory::Write(m_resolutionScansResult[List2] + 1, m_newResX);
			Memory::Write(m_resolutionScansResult[List2] + 6, m_newResY);
			// 1024x768
			Memory::Write(m_resolutionScansResult[List2] + 13, m_newResX);
			Memory::Write(m_resolutionScansResult[List2] + 18, m_newResY);
			// 1280x1024
			Memory::Write(m_resolutionScansResult[List2] + 25, m_newResX);
			Memory::Write(m_resolutionScansResult[List2] + 30, m_newResY);
			// 1600x1200
			Memory::Write(m_resolutionScansResult[List2] + 40, m_newResX);
			Memory::Write(m_resolutionScansResult[List2] + 48, m_newResY);

			// Resolution List 3
			// 800x600
			Memory::Write(m_resolutionScansResult[List3] + 1, m_newResX);
			Memory::Write(m_resolutionScansResult[List3] + 6, m_newResY);
			// 1024x768
			Memory::Write(m_resolutionScansResult[List3] + 13, m_newResX);
			Memory::Write(m_resolutionScansResult[List3] + 18, m_newResY);
			// 1280x1024
			Memory::Write(m_resolutionScansResult[List3] + 25, m_newResX);
			Memory::Write(m_resolutionScansResult[List3] + 30, m_newResY);
			// 1600x1200
			Memory::Write(m_resolutionScansResult[List3] + 40, m_newResX);
			Memory::Write(m_resolutionScansResult[List3] + 48, m_newResY);

			// Resolution List 4
			// 800x600
			Memory::Write(m_resolutionScansResult[List4] + 1, m_newResX);
			Memory::Write(m_resolutionScansResult[List4] + 6, m_newResY);
			// 1024x768
			Memory::Write(m_resolutionScansResult[List4] + 13, m_newResX);
			Memory::Write(m_resolutionScansResult[List4] + 18, m_newResY);
			// 1280x1024
			Memory::Write(m_resolutionScansResult[List4] + 25, m_newResX);
			Memory::Write(m_resolutionScansResult[List4] + 30, m_newResY);
			// 1600x1200
			Memory::Write(m_resolutionScansResult[List4] + 40, m_newResX);
			Memory::Write(m_resolutionScansResult[List4] + 48, m_newResY);
		}

		m_engineDllModule = Memory::GetHandle("Engine.dll");
		m_engineDllModuleName = Memory::GetModuleName(m_engineDllModule);

		m_cameraFOVScanResult = Memory::PatternScan(m_engineDllModule, "D9 05 ?? ?? ?? ?? D8 75 ?? D9 5D ?? 8B 4D");
		if (m_cameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", m_engineDllModuleName.c_str(), m_cameraFOVScanResult - (std::uint8_t*)m_engineDllModule);

			Memory::WriteNOPs(m_cameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(m_cameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newCameraFOV = m_originalCameraFOV * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? C7 85 ?? ?? ?? ?? ?? ?? ?? ?? C6 45 ?? ?? 8D 8D ?? ?? ?? ?? E8 ?? ?? ?? ?? C7 45 ?? ?? ?? ?? ?? 8D 8D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 85 ?? ?? ?? ?? E9 ?? ?? ?? ?? E8");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}
	}

private:
	HMODULE m_dxWrapDllModule = nullptr;
	std::string m_dxWrapDllModuleName = "";
	HMODULE m_engineDllModule = nullptr;
	std::string m_engineDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.7853981634f;

	SafetyHookMid m_cameraFOVHook{};

	bool m_runMultipleInstances = false;
	bool m_skipIntroVideos = false;

	std::vector<uint8_t*> m_resolutionScansResult{};
	uint8_t* m_cameraFOVScanResult = nullptr;
	uint8_t* m_skipIntroVideosScanResult = nullptr;

	enum ResolutionListsIndex
	{
		List1,
		List2,
		List3,
		List4,
		List5
	};

	inline static TigerWoodsPGATour2004Fix* s_instance_ = nullptr;
};

static std::unique_ptr<TigerWoodsPGATour2004Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<TigerWoodsPGATour2004Fix>(hModule);
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