#include "..\..\common\FixBase.hpp"
#include <algorithm>

class UrbanChaosFix final : public FixBase
{
public:
	explicit UrbanChaosFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~UrbanChaosFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "UrbanChaosWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Urban Chaos";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Fallen.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "BF 00 04 00 00 BE 00 03 00 00", "68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 53 FF D6 A1",
		"68 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 55 FF D7 A1 ?? ?? ?? ?? 6A", "A3 ?? ?? ?? ?? A1 ?? ?? ?? ?? 83 F8", "89 3D ?? ?? ?? ?? 89 35 ?? ?? ?? ?? E8");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResList] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Resolution Strings 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Strings1] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Resolution Strings 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Strings2] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Resolution Selection Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResSelection] - reinterpret_cast<std::uint8_t*>(ExeModule()));
			spdlog::info("Width/Height Globals Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - reinterpret_cast<std::uint8_t*>(ExeModule()));

			gResolutionInstructions = reinterpret_cast<std::uintptr_t>(ResolutionScansResult[ResList]);

			gResolutionIndexAddress = Memory::GetPointerFromAddress(ResolutionScansResult[ResSelection] + 1, Memory::PointerMode::Absolute);

			spdlog::info("Resolution Index Address: {:s}+{:x}", ExeName().c_str(), gResolutionIndexAddress - reinterpret_cast<std::uintptr_t>(ExeModule()));

			BuildSystemResolutionList();

			m_resolutionListInitHook = safetyhook::create_mid(ResolutionScansResult[Strings1] + 15, [](SafetyHookContext & ctx)
			{
				s_instance_->AddSystemResolutionsToCombo(reinterpret_cast<HWND>(ctx.ebx));
			});

			m_resolutionListRebuildHook = safetyhook::create_mid(ResolutionScansResult[Strings2] + 15, [](SafetyHookContext& ctx)
			{
				s_instance_->AddSystemResolutionsToCombo(reinterpret_cast<HWND>(ctx.ebp));
			});

			m_resolutionSelectionHook = safetyhook::create_mid(ResolutionScansResult[ResSelection] + 5, [](SafetyHookContext& ctx)
			{
				if (!s_instance_->gResolutionIndexAddress)
				{
					return;
				}

				int selectedIndex = Memory::ReadMem(s_instance_->gResolutionIndexAddress);

				if (selectedIndex > 4)
				{
					HWND hCombo = *reinterpret_cast<HWND*>(ctx.esp + 0x40);

					if (!IsWindow(hCombo))
					{
						spdlog::error("Resolution combo handle is invalid.");
						return;
					}

					LPARAM packed = SendMessageA(hCombo, CB_GETITEMDATA_SAFE, static_cast<WPARAM>(selectedIndex), 0);

					if (packed != CB_ERR)
					{
						int width = s_instance_->UnpackWidth(packed);
						int height = s_instance_->UnpackHeight(packed);

						s_instance_->ApplyResolutionToIndex4(width, height);

						Memory::Write(s_instance_->gResolutionIndexAddress, 4);
					}
					else
					{
						spdlog::error("CB_GETITEMDATA failed for selected index {}.", selectedIndex);
					}
				}
				else if (selectedIndex == 4)
				{
					s_instance_->ApplyResolutionToIndex4(1024, 768);
				}
			});

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadRegister(ctx.edi);
				s_instance_->m_newResY = Memory::ReadRegister(ctx.esi);
				s_instance_->m_newAspectRatio = static_cast<float>(s_instance_->m_newResX) / static_cast<float>(s_instance_->m_newResY);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? A3 ?? ?? ?? ?? 83 E8");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			m_aspectRatioAddress = Memory::GetPointerFromAddress(AspectRatioScanResult + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(AspectRatioScanResult, 10);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->m_newHorizontalRes = 640.0f * s_instance_->m_aspectRatioScale;
				*reinterpret_cast<float*>(s_instance_->m_aspectRatioAddress) = s_instance_->m_newHorizontalRes;
			});
		}
		else
		{
			spdlog::info("Failed to locate the aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "C7 46 ?? ?? ?? ?? ?? 84 C0");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			m_newCameraFOV = (uint32_t)(m_originalCameraFOV / m_fovFactor);

			Memory::Write(CameraFOVScanResult + 3, m_newCameraFOV);
		}
		else
		{
			spdlog::error("Failed to locate the camera FOV instruction memory address.");
			return;
		}

		auto TextureResolutionFixScanResult = Memory::PatternScan(ExeModule(), "80 CC ?? 89 45 ?? 8B 45");
		if (TextureResolutionFixScanResult)
		{
			spdlog::info("Texture Resolution Fix Scan: Address is {:s}+{:x}", ExeName().c_str(), TextureResolutionFixScanResult - (std::uint8_t*)ExeModule());

			Memory::PatchBytes(TextureResolutionFixScanResult + 2, "\x00");
		}
		else
		{
			spdlog::error("Failed to locate texture resolution fix scan memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr uint32_t m_originalCameraFOV = 122880;	

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_resolutionListInitHook{};
	SafetyHookMid m_resolutionListRebuildHook{};
	SafetyHookMid m_resolutionSelectionHook{};
	SafetyHookMid m_aspectRatioHook{};

	uint32_t m_newCameraFOV = 0;

	uintptr_t m_aspectRatioAddress = 0;
	float m_newHorizontalRes = 0.0f;
	
	enum ResolutionScansIndex
	{
		ResList,
		Strings1,
		Strings2,
		ResSelection,
		WidthHeight
	};

	std::vector<std::pair<int, int>> gSystemResolutions;

	std::uintptr_t gResolutionInstructions = 0;
	std::uintptr_t gResolutionIndexAddress = 0;

	static constexpr int WM_USER_CB_ADDSTRING = 0x014A;
	static constexpr int WM_USER_CB_SETCURSEL = 0x014E;
	static constexpr int CB_FINDSTRINGEXACT_SAFE = 0x0158;
	static constexpr int CB_SETITEMDATA_SAFE = 0x0151;
	static constexpr int CB_GETITEMDATA_SAFE = 0x0150;

	LPARAM PackResolution(int width, int height)
	{
		return (static_cast<LPARAM>(width) << 16) | static_cast<LPARAM>(height & 0xFFFF);
	}

	int UnpackWidth(LPARAM packed)
	{
		return static_cast<int>((packed >> 16) & 0xFFFF);
	}

	int UnpackHeight(LPARAM packed)
	{
		return static_cast<int>(packed & 0xFFFF);
	}

	void ApplyResolutionToIndex4(int width, int height)
	{
		if (!gResolutionInstructions)
		{
			return;
		}

		Memory::Write(gResolutionInstructions + 1, width);
		Memory::Write(gResolutionInstructions + 6, height);
	}

	void BuildSystemResolutionList()
	{
		gSystemResolutions.clear();

		DEVMODEA dm = {};
		dm.dmSize = sizeof(dm);

		for (DWORD i = 0; EnumDisplaySettingsA(nullptr, i, &dm); i++)
		{
			int width = static_cast<int>(dm.dmPelsWidth);
			int height = static_cast<int>(dm.dmPelsHeight);

			if (width <= 0 || height <= 0)
			{
				continue;
			}

			auto res = std::make_pair(width, height);

			if (std::find(gSystemResolutions.begin(), gSystemResolutions.end(), res) == gSystemResolutions.end())
			{
				gSystemResolutions.push_back(res);
			}
		}

		std::sort(gSystemResolutions.begin(), gSystemResolutions.end(), [](const auto& a, const auto& b)
		{
			if (a.first != b.first)
			{
				return a.first < b.first;
			}

			return a.second < b.second;
		});
	}

	void AddSystemResolutionsToCombo(HWND hCombo)
	{
		if (!hCombo)
		{
			return;
		}

		int wantedIndex = -1;

		for (const auto& [width, height] : gSystemResolutions)
		{
			std::string text = std::to_string(width) + " x " + std::to_string(height);

			LRESULT existing = SendMessageA(hCombo, CB_FINDSTRINGEXACT_SAFE, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str()));

			int index = -1;

			if (existing == CB_ERR)
			{
				index = static_cast<int>(SendMessageA(hCombo, WM_USER_CB_ADDSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(text.c_str())));
			}
			else
			{
				index = static_cast<int>(existing);
			}

			if (index >= 0)
			{
				SendMessageA(hCombo, CB_SETITEMDATA_SAFE, static_cast<WPARAM>(index), PackResolution(width, height));
			}
		}

		if (wantedIndex >= 0)
		{
			SendMessageA(hCombo, WM_USER_CB_SETCURSEL, static_cast<WPARAM>(wantedIndex), 0);

			if (gResolutionIndexAddress)
			{
				Memory::Write(gResolutionIndexAddress, wantedIndex);
			}
		}
	}

	inline static UrbanChaosFix* s_instance_ = nullptr;
};

static std::unique_ptr<UrbanChaosFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<UrbanChaosFix>(hModule);
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