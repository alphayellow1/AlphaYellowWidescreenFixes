#include "..\..\common\FixBase.hpp"

class WCS2003Fix final : public FixBase
{
public:
	explicit WCS2003Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WCS2003Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "WorldChampionshipSnooker2003WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "World Championship Snooker 2003";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "wcs2003.exe");
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
		ResolutionScansResult = Memory::PatternScan(ExeModule(), "3B D1 0F 85 ?? ?? ?? ?? 3D",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8B CE E8 ?? ?? ?? ?? 8B 54 24 ?? 52 8D 44 24 ?? 68 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 83 C4 ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8B CE E8 ?? ?? ?? ?? 8B 54 24",
		"C7 02 ?? ?? ?? ?? 8B 04 24", "A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 81 EC");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("FMVs Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[FMVs] - (std::uint8_t*)ExeModule());
			spdlog::info("Startup Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[StartupRes] - (std::uint8_t*)ExeModule());
			spdlog::info("Width/Height Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock], 30);

			m_resolutionWidthAddress = Memory::GetPointerFromAddress(ResolutionScansResult[WidthHeight] + 7, Memory::PointerMode::Absolute);
			m_resolutionHeightAddress = Memory::GetPointerFromAddress(ResolutionScansResult[WidthHeight] + 1, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(s_instance_->m_resolutionWidthAddress);
				s_instance_->m_newResY = Memory::ReadMem(s_instance_->m_resolutionHeightAddress);
				s_instance_->WriteStaticRes();
				s_instance_->m_resolutionHook.disable();
			});			
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 89 4C 24 ?? C7 44 24");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = m_originalCameraFOV * m_fovFactor;

			Memory::Write(CameraFOVScanResult + 4, m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto MultipleInstancesScanResult = Memory::PatternScan(ExeModule(), "75 ?? A1 ?? ?? ?? ?? 50 FF 15");
			if (MultipleInstancesScanResult)
			{
				spdlog::info("Multiple Instances Check Scan: Address is {:s}+{:x}", ExeName().c_str(), MultipleInstancesScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(MultipleInstancesScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instances check memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.3490999937f;

	SafetyHookMid m_resolutionHook{};

	bool m_runMultipleInstances = false;

	std::vector<std::uint8_t*> ResolutionScansResult;

	uintptr_t m_resolutionWidthAddress = 0;
	uintptr_t m_resolutionHeightAddress = 0;

	void WriteStaticRes()
	{
		Memory::Write(ResolutionScansResult[FMVs] + 6, m_newResX);
		Memory::Write(ResolutionScansResult[FMVs] + 1, m_newResY);
		Memory::Write(ResolutionScansResult[FMVs] + 56, m_newResX);
		Memory::Write(ResolutionScansResult[FMVs] + 51, m_newResY);
		Memory::Write(ResolutionScansResult[FMVs] + 106, m_newResX);
		Memory::Write(ResolutionScansResult[FMVs] + 101, m_newResY);

		Memory::Write(ResolutionScansResult[StartupRes] + 2, m_newResX);
		Memory::Write(ResolutionScansResult[StartupRes] + 12, m_newResY);
		Memory::Write(ResolutionScansResult[StartupRes] + 30, m_newResX);
		Memory::Write(ResolutionScansResult[StartupRes] + 25, m_newResY);
		Memory::Write(ResolutionScansResult[StartupRes] + 45, m_newResX);
		Memory::Write(ResolutionScansResult[StartupRes] + 40, m_newResY);
	}

	enum ResolutionScansIndex
	{
		ListUnlock,
		FMVs,
		StartupRes,
		WidthHeight
	};

	inline static WCS2003Fix* s_instance_ = nullptr;
};

static std::unique_ptr<WCS2003Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<WCS2003Fix>(hModule);
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