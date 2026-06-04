#include "..\..\common\FixBase.hpp"
#include <set>

class GotchaExtremePaintballFix final : public FixBase
{
public:
	explicit GotchaExtremePaintballFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~GotchaExtremePaintballFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "GotchaExtremePaintballWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Gotcha: Extreme Paintball";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Gotcha.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "HipfireFOVFactor", m_hipfireFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFOVFactor", m_zoomFOVFactor);
		spdlog_confparse(m_hipfireFOVFactor);
		spdlog_confparse(m_zoomFOVFactor);
	}

	void ApplyFix() override
	{
		PatchResolutionListToSystemModes();

		HMODULE vision71DllModule = Memory::GetHandle("vision71.dll");
		std::string vision71DllModuleName = Memory::GetModuleName(vision71DllModule);

		auto ResolutionScanResult = Memory::PatternScan(vision71DllModule, "8B 5C 24 ?? 55 8B 6C 24 ?? 8D 04 2A");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", vision71DllModuleName.c_str(), ResolutionScanResult - (std::uint8_t*)vision71DllModule);

			m_resolutionWidthHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(ctx.esp + 0xC);
			});

			m_resolutionHeightHook = safetyhook::create_mid(ResolutionScanResult + 38, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResY = Memory::ReadMem(ctx.esp + 0x1C);

				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteStaticFOV();
			});
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "B8 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 9E",
		"8B 80 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 89 8E");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Hipfire Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire] - (std::uint8_t*)ExeModule());
			spdlog::info("Zoom FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Zoom] - (std::uint8_t*)ExeModule());			

			Memory::WriteNOPs(CameraFOVScansResult[Zoom], 6);

			m_zoomFOVHook = safetyhook::create_mid(CameraFOVScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = Memory::ReadMem(ctx.eax + 0x3A8);
				s_instance_->m_newZoomFOV = Maths::CalculateNewFOV_DegBased(fCurrentZoomFOV, s_instance_->m_aspectRatioScale) / s_instance_->m_zoomFOVFactor;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newZoomFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalHipfireFOV = 90.0f;

	SafetyHookMid m_resolutionWidthHook{};
	SafetyHookMid m_resolutionHeightHook{};
	SafetyHookMid m_zoomFOVHook{};

	float m_hipfireFOVFactor = 0.0f;
	float m_zoomFOVFactor = 0.0f;

	float m_newHipfireFOV = 0.0f;
	float m_newZoomFOV = 0.0f;
	
	std::vector<std::uint8_t*> CameraFOVScansResult;

	enum CameraFOVInstructionsIndex
	{
		Hipfire,
		Zoom
	};

	struct ResolutionEntry
	{
		uint32_t Width;
		uint32_t Height;
	};

	std::vector<ResolutionEntry> gResolutionTable;

	std::vector<ResolutionEntry> GetAvailableDisplayResolutions()
	{
		std::set<std::pair<uint32_t, uint32_t>> uniqueResolutions;

		DEVMODEW devMode{};
		devMode.dmSize = sizeof(devMode);

		for (DWORD modeIndex = 0; EnumDisplaySettingsW(nullptr, modeIndex, &devMode); ++modeIndex)
		{
			uint32_t width = static_cast<uint32_t>(devMode.dmPelsWidth);
			uint32_t height = static_cast<uint32_t>(devMode.dmPelsHeight);

			uniqueResolutions.insert({ width, height });

			ZeroMemory(&devMode, sizeof(devMode));
			devMode.dmSize = sizeof(devMode);
		}

		std::vector<ResolutionEntry> result;
		result.reserve(uniqueResolutions.size());

		for (const auto& [width, height] : uniqueResolutions)
		{
			result.push_back({ width, height });
		}

		std::sort(result.begin(), result.end(), [](const ResolutionEntry& a, const ResolutionEntry& b)
			{
				if (a.Width != b.Width)
				{
					return a.Width < b.Width;
				}

				return a.Height < b.Height;
			});

		return result;
	}

	void PatchResolutionListToSystemModes()
	{
		gResolutionTable = GetAvailableDisplayResolutions();

		if (gResolutionTable.empty())
		{
			spdlog::error("No display resolutions were found.");
			return;
		}

		if (gResolutionTable.size() > 127)
		{
			spdlog::warn("Resolution count {} is too high for existing cmp eax, imm8 loop. Clamping to 127.", gResolutionTable.size());

			gResolutionTable.resize(127);
		}

		const uintptr_t tableStart = reinterpret_cast<uintptr_t>(gResolutionTable.data());
		const uintptr_t tableEnd = tableStart + (gResolutionTable.size() * sizeof(ResolutionEntry));

		if (tableStart > 0xFFFFFFFF || tableEnd > 0xFFFFFFFF)
		{
			spdlog::error("Resolution table address does not fit in 32-bit immediate. Start=0x{:X}, End=0x{:X}", tableStart, tableEnd);

			return;
		}

		const uint32_t tableStart32 = static_cast<uint32_t>(tableStart);
		const uint32_t tableHeight32 = static_cast<uint32_t>(tableStart + offsetof(ResolutionEntry, Height));
		const uint32_t tableEnd32 = static_cast<uint32_t>(tableEnd);
		const uint8_t resolutionCount8 = static_cast<uint8_t>(gResolutionTable.size());

		std::uint8_t* resolutionLoopStartScan = Memory::PatternScan(ExeModule(), "BF ?? ?? ?? ?? BB 15 00 00 00 8B FF 8B 4F 04 8B 17");

		if (!resolutionLoopStartScan)
		{
			spdlog::error("Failed to locate resolution loop start instruction.");
			return;
		}

		spdlog::info("Resolution loop start instruction: Address is {:s}+{:x}", ExeName().c_str(), resolutionLoopStartScan - reinterpret_cast<std::uint8_t*>(ExeModule()));

		Memory::Write(resolutionLoopStartScan + 1, tableStart32);

		std::uint8_t* resolutionLoopEndScan = Memory::PatternScan(ExeModule(), "83 C7 08 81 FF ?? ?? ?? ?? 7C BF");

		if (!resolutionLoopEndScan)
		{
			spdlog::error("Failed to locate resolution loop end instruction.");
			return;
		}

		spdlog::info("Resolution loop end instruction: Address is {:s}+{:x}", ExeName().c_str(), resolutionLoopEndScan - reinterpret_cast<std::uint8_t*>(ExeModule()));

		Memory::Write(resolutionLoopEndScan + 5, tableEnd32);

		std::uint8_t* applyResolutionLookupScan = Memory::PatternScan(ExeModule(), "8D 34 C5 ?? ?? ?? ?? 8B 06 3B C1");

		if (!applyResolutionLookupScan)
		{
			spdlog::error("Failed to locate Apply resolution lookup.");
			return;
		}

		spdlog::info("Apply resolution lookup: Address is {:s}+{:x}", ExeName().c_str(), applyResolutionLookupScan - reinterpret_cast<std::uint8_t*>(ExeModule()));

		Memory::Write(applyResolutionLookupScan + 3, tableStart32);

		std::uint8_t* currentResolutionLookupScan = Memory::PatternScan(ExeModule(), "39 0C C5 ?? ?? ?? ?? 75 09 39 14 C5 ?? ?? ?? ?? 74 08 40 83 F8 ?? 7C E8");

		if (!currentResolutionLookupScan)
		{
			spdlog::error("Failed to locate current resolution lookup.");
			return;
		}

		spdlog::info("Current resolution lookup: Address is {:s}+{:x}", ExeName().c_str(), currentResolutionLookupScan - reinterpret_cast<std::uint8_t*>(ExeModule()));

		Memory::Write(currentResolutionLookupScan + 3, tableStart32);
		Memory::Write(currentResolutionLookupScan + 12, tableHeight32);
		Memory::Write(currentResolutionLookupScan + 21, resolutionCount8);
	}

	void WriteStaticFOV()
	{
		m_newHipfireFOV = Maths::CalculateNewFOV_DegBased(m_originalHipfireFOV, m_aspectRatioScale) * m_hipfireFOVFactor;

		Memory::Write(CameraFOVScansResult[Hipfire] + 1, m_newHipfireFOV);
	}

	inline static GotchaExtremePaintballFix* s_instance_ = nullptr;
};

static std::unique_ptr<GotchaExtremePaintballFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<GotchaExtremePaintballFix>(hModule);
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