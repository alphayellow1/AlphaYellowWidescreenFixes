#include "..\..\common\FixBase.hpp"
#include <set>

class FootballGenerationFix final : public FixBase
{
public:
	explicit FootballGenerationFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~FootballGenerationFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "FootballGenerationWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3";
	}

	const char* TargetName() const override
	{
		return "Football Generation";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Game.exe") ||
		Util::stringcmp_caseless(exeName, "Setup.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "Setup.exe"))
		{
			BuildSystemResolutionList();

			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "B9 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B CE E8 ?? ?? ?? ?? 68 ?? ?? ?? ??");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult + 17 - (std::uint8_t*)ExeModule());

				m_resolutionListUnlockHook = safetyhook::create_mid(ResolutionListUnlockScanResult + 17, [](SafetyHookContext& ctx)
				{
					auto setupThisPtr = static_cast<std::uintptr_t>(ctx.esi);

					HWND combo = *reinterpret_cast<HWND*>(setupThisPtr + 0x348);

					if (!s_instance_->g_resolution_strings.empty() && IsWindow(combo))
					{
						s_instance_->AddSystemResolutionsToCombo(combo);

						if (s_instance_->g_desktop_mode_index >= 0)
						{
							*reinterpret_cast<int*>((uint8_t*)s_instance_->ExeModule() + 0x4CF64) = s_instance_->g_desktop_mode_index;
						}

						ctx.eip = (uintptr_t)s_instance_->ExeModule() + 0x1816;
					}
				});
			}			
		}

		if (Util::stringcmp_caseless(ExeName(), "Game.exe"))
		{
			auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 02 A3 ?? ?? ?? ?? 8B 42");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

				m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.edx);
					int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				});
			}
			else
			{
				spdlog::info("Failed to locate the resolution instructions scan memory address.");
				return;
			}

			auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D9 5C 24 ?? EB ?? 8B 46");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(AspectRatioScanResult, 6);

				m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
				{
					s_instance_->m_newAspectRatio2 = 0.75f / s_instance_->m_aspectRatioScale;

					FPU::FMUL(s_instance_->m_newAspectRatio2);
				});
			}
			else
			{
				spdlog::info("Cannot locate the aspect ratio instruction memory address.");
				return;
			}

			auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 4E ?? 6A ?? 89 4F", "8B 56 ?? 8A 46 ?? 8B 4E ?? 52 50 E8 ?? ?? ?? ?? 5F");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVScansResult[FOV1], 3);

				m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVMidHook(ctx.esi + 0x24, ctx.ecx, s_instance_->m_fovFactor);
				});

				Memory::WriteNOPs(CameraFOVScansResult[FOV2], 3);

				m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVMidHook(ctx.esi + 0x30, ctx.edx);
				});
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionListUnlockHook{};
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum ResolutionInstructionsIndex
	{
		Res1,
		Res2
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};	

	struct Mode
	{
		DWORD width;
		DWORD height;
		DWORD bpp;

		bool operator<(const Mode& other) const
		{
			if (width != other.width)
			{
				return width < other.width;
			}

			if (height != other.height)
			{
				return height < other.height;
			}

			return bpp < other.bpp;
		}
	};

	std::vector<std::string> g_resolution_strings;
	int g_desktop_mode_index = -1;

	void BuildSystemResolutionList()
	{
		std::set<Mode> unique_modes;

		DEVMODEA current{};
		current.dmSize = sizeof(current);

		DWORD current_width = 0;
		DWORD current_height = 0;
		DWORD current_bpp = 0;

		if (EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &current))
		{
			current_width = current.dmPelsWidth;
			current_height = current.dmPelsHeight;
			current_bpp = current.dmBitsPerPel;
		}

		for (DWORD i = 0;; ++i)
		{
			DEVMODEA dm{};
			dm.dmSize = sizeof(dm);

			if (!EnumDisplaySettingsA(nullptr, i, &dm))
			{
				break;
			}

			if (dm.dmPelsWidth == 0 || dm.dmPelsHeight == 0 || dm.dmBitsPerPel == 0)
			{
				continue;
			}

			unique_modes.insert({ dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel });
		}

		g_resolution_strings.clear();
		g_desktop_mode_index = -1;

		int index = 0;

		for (const auto& mode : unique_modes)
		{
			char buffer[64]{};

			std::snprintf(buffer, sizeof(buffer), "%lux%lu %lu bits", mode.width, mode.height, mode.bpp);

			g_resolution_strings.emplace_back(buffer);

			if (mode.width == current_width && mode.height == current_height && mode.bpp == current_bpp)
			{
				g_desktop_mode_index = index;
			}

			++index;
		}
	}

	void AddSystemResolutionsToCombo(HWND combo)
	{
		if (!IsWindow(combo))
		{
			return;
		}

		SendMessageA(combo, CB_RESETCONTENT, 0, 0);

		for (const auto& resolution : g_resolution_strings)
		{
			SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(resolution.c_str()));
		}

		if (g_desktop_mode_index >= 0)
		{
			SendMessageA(combo, CB_SETCURSEL, static_cast<WPARAM>(g_desktop_mode_index), 0);
		}
	}

	void CameraFOVMidHook(uintptr_t SourceAddress, uintptr_t& DestAddress, float fovFactor = 1.0f)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(SourceAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;

		DestAddress = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static FootballGenerationFix* s_instance_ = nullptr;
};

static std::unique_ptr<FootballGenerationFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<FootballGenerationFix>(hModule);
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