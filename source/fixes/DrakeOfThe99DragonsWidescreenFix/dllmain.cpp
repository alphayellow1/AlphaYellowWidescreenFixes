#include "..\..\common\FixBase.hpp"
#include "WinReg.hpp"
#include <set>

class DrakeOfThe99DragonsFix final : public FixBase
{
public:
	explicit DrakeOfThe99DragonsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DrakeOfThe99DragonsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DrakeOfThe99DragonsWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Drake of the 99 Dragons";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Drake.exe") ||
		Util::stringcmp_caseless(exeName, "drakeshell.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "drakeshell.exe"))
		{
			Memory::Write((uintptr_t)ExeModule() + 0xC4A4 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC501 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC53C + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC577 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC5B2 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC5ED + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC628 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC663 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC69E + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC6D9 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC714 + 1, 0x004160B4);
			Memory::Write((uintptr_t)ExeModule() + 0xC74B + 1, 0x004160B4);

			Memory::WriteNOPs((uint8_t*)ExeModule() + 0xC4BC, 6);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x1952, 13);

			m_registryCreateKeyHook = safetyhook::create_mid((uintptr_t)ExeModule() + 0x195F, [](SafetyHookContext& ctx)
			{
				auto phkResult = reinterpret_cast<PHKEY>(ctx.eax);
				auto lpSubKey = reinterpret_cast<LPCSTR>(ctx.ecx);
				auto hRootKey = reinterpret_cast<HKEY>(ctx.edx);

				DWORD disposition = 0;

				LSTATUS status = RegCreateKeyExA(hRootKey, lpSubKey, 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, phkResult, &disposition);

				ctx.eax = static_cast<std::uintptr_t>(status);
			});

			auto ResolutionListUnlockScanResult = Memory::PatternScan(ExeModule(), "81 FB ?? ?? ?? ?? 74 ?? 81 FB ?? ?? ?? ?? 74 ?? 81 FB ?? ?? ?? ?? 74");
			if (ResolutionListUnlockScanResult)
			{
				spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScanResult, 104);
			}
			else
			{
				spdlog::error("Failed to locate resolution list unlock scan memory address.");
				return;
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Drake.exe"))
		{
			auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 6A ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 50 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8D 8C 24",
			"8B BC 24 ?? ?? ?? ?? 8B 9C 24 ?? ?? ?? ?? 8B B4 24");
			if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
			{
				spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResList] - reinterpret_cast<std::uint8_t*>(ExeModule()));
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - reinterpret_cast<std::uint8_t*>(ExeModule()));

				auto modes = GetSystemDisplayModes();

				m_resolutionWidths = JoinWidths(modes);
				m_resolutionHeights = JoinHeights(modes);

				const char* cNewResolutionWidthsList = m_resolutionWidths.c_str();
				const char* cNewResolutionHeightsList = m_resolutionHeights.c_str();

				Memory::Write(ResolutionScansResult[ResList] + 1, cNewResolutionWidthsList);
				Memory::Write(ResolutionScansResult[ResList] + 50, cNewResolutionHeightsList);

				m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0xE4);
					int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0xE8);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				});
			}

			auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 81 ?? ?? ?? ?? 8B 44 24");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(AspectRatioScanResult, 6);

				m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
				{
					FPU::FADD(s_instance_->m_newAspectRatio);
				});
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction memory address.");
				return;
			}

			auto CameraFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "8B 86 ?? ?? ?? ?? 50 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? 5F", "8B 86 ?? ?? ?? ?? 8D 4C 24 ?? 89 44 24");
			if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV1] - (std::uint8_t*)ExeModule());
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[FOV2] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV1], 6);

				m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV1], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.esi + 0x2F0);

					if (fCurrentCameraFOV1 == 1.450000048f)
					{
						s_instance_->m_newCameraFOV1 = fCurrentCameraFOV1 * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV1 = fCurrentCameraFOV1;
					}

					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV1);
				});

				Memory::WriteNOPs(CameraFOVInstructionsScansResult[FOV2], 6);

				m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVInstructionsScansResult[FOV2], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.esi + 0x374);

					if (fCurrentCameraFOV2 == 1.087500095f)
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale) * s_instance_->m_fovFactor;
					}
					else
					{
						s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale);
					}

					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV2);
				});
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_registryCreateKeyHook{};
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	std::string m_resolutionWidths = "";
	std::string m_resolutionHeights = "";

	enum ResolutionInstructionsIndex
	{
		ResList,
		WidthHeight
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2
	};

	struct DisplayMode
	{
		DWORD width;
		DWORD height;
		DWORD bpp;
		DWORD frequency;
	};

	static std::vector<DisplayMode> GetSystemDisplayModes()
	{
		std::vector<DisplayMode> modes;
		std::set<std::pair<DWORD, DWORD>> seenResolutions;

		DEVMODEA devMode = {};
		devMode.dmSize = sizeof(DEVMODEA);

		for (DWORD i = 0; EnumDisplaySettingsA(nullptr, i, &devMode); ++i)
		{
			DWORD width = devMode.dmPelsWidth;
			DWORD height = devMode.dmPelsHeight;
			DWORD bpp = devMode.dmBitsPerPel;
			DWORD frequency = devMode.dmDisplayFrequency;

			if (bpp != 16 && bpp != 32)
			{
				continue;
			}

			auto key = std::make_pair(width, height);
			if (seenResolutions.insert(key).second)
			{
				modes.push_back({ width, height, bpp, frequency });
			}
		}

		std::sort(modes.begin(), modes.end(), [](const DisplayMode& a, const DisplayMode& b)
			{
				if (a.width != b.width)
				{
					return a.width < b.width;
				}

				return a.height < b.height;
			});

		return modes;
	}

	static std::string JoinWidths(const std::vector<DisplayMode>& modes)
	{
		std::string result;

		for (size_t i = 0; i < modes.size(); ++i)
		{
			if (i > 0)
			{
				result += ":";
			}

			result += std::to_string(modes[i].width);
		}

		return result;
	}

	static std::string JoinHeights(const std::vector<DisplayMode>& modes)
	{
		std::string result;

		for (size_t i = 0; i < modes.size(); ++i)
		{
			if (i > 0)
			{
				result += ":";
			}

			result += std::to_string(modes[i].height);
		}

		return result;
	}

	void CameraFOVMidHook(uintptr_t SourceAddress, uintptr_t& DestAddress, float fovFactor = 1.0f)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(SourceAddress);

		m_newCameraFOV = Maths::CalculateNewFOV_DegBased(fCurrentCameraFOV, m_aspectRatioScale) * fovFactor;

		DestAddress = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	inline static DrakeOfThe99DragonsFix* s_instance_ = nullptr;
};

static std::unique_ptr<DrakeOfThe99DragonsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<DrakeOfThe99DragonsFix>(hModule);
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