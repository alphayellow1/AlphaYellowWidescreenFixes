#include "..\..\common\FixBase.hpp"

class ApacheLongbowAssaultFix final : public FixBase
{
public:
	explicit ApacheLongbowAssaultFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ApacheLongbowAssaultFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "ApacheLongbowAssaultWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Apache Longbow Assault";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Longbow.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "8B 46 ?? 83 F8 ?? 8B 5E", "8B 46 ?? 83 EC ?? 85 C0 53 55 57 74",
		"89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? A3 ?? ?? ?? ?? BD");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Scroll Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListScroll] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			m_resolutionListUnlockHook = safetyhook::create_mid(ResolutionScansResult[ResListUnlock], [](SafetyHookContext& ctx)
			{
				const auto exe = reinterpret_cast<uintptr_t>(s_instance_->ExeModule());

				constexpr uintptr_t RESOLUTION_COMBO_PTR_RVA = 0x1CEF80;
				constexpr uintptr_t RESOLUTION_LIST_RETURN_RVA = 0x96291;

				constexpr uintptr_t CURRENT_WIDTH_RVA = 0x1D202C;
				constexpr uintptr_t CURRENT_HEIGHT_RVA = 0x1D2030;
				constexpr uintptr_t CURRENT_BPP_RVA = 0x1D2038;

				constexpr int ITEM_STRIDE = 0x100;
				constexpr int MAX_MODES = 0x7F;
				constexpr int ITEM_BUFFER_SIZE = MAX_MODES * ITEM_STRIDE;
				constexpr int MAX_VISIBLE_ROWS = 16;

				struct Mode
				{
					DWORD width;
					DWORD height;
					DWORD bpp;
				};

				static void* s_item_buffer = nullptr;
				static int s_resolution_count = 0;

				void* combo = *reinterpret_cast<void**>(exe + RESOLUTION_COMBO_PTR_RVA);

				if (combo == nullptr)
				{
				    s_resolution_count = 0;
				    ctx.eip = exe + RESOLUTION_LIST_RETURN_RVA;
				    return;
				}

				if (!s_item_buffer)
				{
				   s_item_buffer = VirtualAlloc(nullptr, ITEM_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				}

				if (!s_item_buffer)
				{
				    s_resolution_count = 0;
					ctx.eip = exe + RESOLUTION_LIST_RETURN_RVA;
					return;
				}

				std::memset(s_item_buffer, 0, ITEM_BUFFER_SIZE);

				const uintptr_t combo_addr = reinterpret_cast<uintptr_t>(combo);

				// The combo item text buffer is stored at combo + 0x10
				*reinterpret_cast<void**>(combo_addr + 0x10) = s_item_buffer;

				std::vector<Mode> modes;
				modes.reserve(128);

				DEVMODEA dm{};
				DWORD mode_index = 0;

				while (static_cast<int>(modes.size()) < MAX_MODES)
				{
					std::memset(&dm, 0, sizeof(dm));
					dm.dmSize = sizeof(dm);

					if (!EnumDisplaySettingsA(nullptr, mode_index, &dm))
					{
						break;
					}

					++mode_index;

					if (dm.dmPelsWidth == 0 || dm.dmPelsHeight == 0 || dm.dmBitsPerPel == 0)
					{
						continue;
					}

					bool duplicate = false;

					for (const Mode& existing : modes)
					{
						if (existing.width == dm.dmPelsWidth && existing.height == dm.dmPelsHeight && existing.bpp == dm.dmBitsPerPel)
						{
							duplicate = true;
							break;
						}
					}

					if (duplicate)
					{
						continue;
					}

					modes.push_back({ dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel});
				}

				std::sort(modes.begin(), modes.end(), [](const Mode& a, const Mode& b)
				{
					if (a.width != b.width)
					{
					    return a.width < b.width;
					}

					if (a.height != b.height)
					{
					    return a.height < b.height;
					}

					return a.bpp < b.bpp;
				});

				const DWORD current_width = *reinterpret_cast<DWORD*>(exe + CURRENT_WIDTH_RVA);

				const DWORD current_height = *reinterpret_cast<DWORD*>(exe + CURRENT_HEIGHT_RVA);

				const DWORD current_bpp = *reinterpret_cast<DWORD*>(exe + CURRENT_BPP_RVA);

				int selected_index = -1;
				int index = 0;

				for (const Mode& mode : modes)
				{
					if (index >= MAX_MODES)
					{
					    break;
					}

					char text[64]{};

					std::snprintf(text, sizeof(text), "%u x %u x %u", mode.width, mode.height, mode.bpp);

					uint8_t* item_base = *reinterpret_cast<uint8_t**>(combo_addr + 0x10);
					uint8_t* slot = item_base + index * ITEM_STRIDE;

					std::memset(slot, 0, ITEM_STRIDE);

					strncpy_s(reinterpret_cast<char*>(slot), ITEM_STRIDE, text, _TRUNCATE);

					if (mode.width == current_width && mode.height == current_height && mode.bpp == current_bpp)
					{
						selected_index = index;
					}

					++index;
				}

				if (index == 0)
				{
					char text[64]{};

					std::snprintf(text, sizeof(text), "%u x %u x %u", current_width, current_height, current_bpp);

					uint8_t* item_base = *reinterpret_cast<uint8_t**>(combo_addr + 0x10);
					uint8_t* slot = item_base;

					std::memset(slot, 0, ITEM_STRIDE);

					strncpy_s(reinterpret_cast<char*>(slot), ITEM_STRIDE, text, _TRUNCATE);

					selected_index = 0;
					index = 1;
				}

				s_resolution_count = index;

				// combo + 0x0C = total item count
				*reinterpret_cast<int*>(combo_addr + 0x0C) = s_resolution_count;

				const int visible_rows = std::min(s_resolution_count, MAX_VISIBLE_ROWS);

				// combo + 0x1C = visible rows
				*reinterpret_cast<int*>(combo_addr + 0x1C) = visible_rows;

				int old_scroll_start = *reinterpret_cast<int*>(combo_addr + 0x14);
				const int max_scroll = std::max(0, s_resolution_count - visible_rows);

				if (old_scroll_start < 0)
				{
				    old_scroll_start = 0;
				}

				if (old_scroll_start > max_scroll)
				{
				    old_scroll_start = max_scroll;
				}

				// combo + 0x14 = scroll/start index
				*reinterpret_cast<int*>(combo_addr + 0x14) = old_scroll_start;

				const int row_height = *reinterpret_cast<int*>(combo_addr + 0x18);
				const int top_y = *reinterpret_cast<int*>(combo_addr + 0x28);

				// combo + 0x30 = bottom y
				*reinterpret_cast<int*>(combo_addr + 0x30) = top_y + row_height * visible_rows;

				// combo + 0x48 = hover/highlight index
				*reinterpret_cast<int*>(combo_addr + 0x48) = -1;

				// combo + 0x4C = selected index
				*reinterpret_cast<int*>(combo_addr + 0x4C) = selected_index;

				ctx.eip = exe + RESOLUTION_LIST_RETURN_RVA;
			});

			m_resolutionListScrollHook = safetyhook::create_mid(ResolutionScansResult[ResListScroll], [](SafetyHookContext& ctx)
			{
				const auto exe = reinterpret_cast<uintptr_t>(s_instance_->ExeModule());

				constexpr uintptr_t RESOLUTION_COMBO_PTR_RVA = 0x1CEF80;

				void* resolution_combo = *reinterpret_cast<void**>(exe + RESOLUTION_COMBO_PTR_RVA);

				if (!resolution_combo)
				{
					return;
				}

				if (reinterpret_cast<void*>(ctx.esi) != resolution_combo)
				{
					return;
				}

				const uintptr_t combo_addr = reinterpret_cast<uintptr_t>(resolution_combo);

				int* total_count  = reinterpret_cast<int*>(combo_addr + 0x0C);
				int* scroll_start = reinterpret_cast<int*>(combo_addr + 0x14);
				int* visible_rows = reinterpret_cast<int*>(combo_addr + 0x1C);

				if (*total_count <= *visible_rows)
				{
					return;
				}

				static DWORD s_last_scroll_tick = 0;

				const DWORD now = GetTickCount();

				if (now - s_last_scroll_tick < 100)
				{
					return;
				}

				int delta = 0;
				bool jump_top = false;
				bool jump_bottom = false;

				if (GetAsyncKeyState(VK_UP) & 0x8000)
				{
					delta -= 1;
				}

				if (GetAsyncKeyState(VK_DOWN) & 0x8000)
				{
					delta += 1;
				}

				if (GetAsyncKeyState(VK_PRIOR) & 0x8000)
				{
					delta -= *visible_rows;
				}

				if (GetAsyncKeyState(VK_NEXT) & 0x8000)
				{
					delta += *visible_rows;
				}

				if (GetAsyncKeyState(VK_HOME) & 0x8000)
				{
				    jump_top = true;
				}

				if (GetAsyncKeyState(VK_END) & 0x8000)
				{
				    jump_bottom = true;
				}

				if (delta == 0 && !jump_top && !jump_bottom)
				{
					return;
				}

				const int max_scroll = std::max(0, *total_count - *visible_rows);

				int new_scroll = *scroll_start;

				if (jump_top)
				{
				    new_scroll = 0;
				}
				else if (jump_bottom)
				{
				    new_scroll = max_scroll;
				}
				else
				{
					new_scroll += delta;

					if (new_scroll < 0)
					{
					    new_scroll = 0;
					}

					if (new_scroll > max_scroll)
					{
					   new_scroll = max_scroll;
					}
				}

				if (new_scroll != *scroll_start)
				{
					*scroll_start = new_scroll;
					*reinterpret_cast<int*>(combo_addr + 0x48) = -1;
				}

				s_last_scroll_tick = now;
			});

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
			{
				const int& iCurrentWidth = Memory::ReadRegister(ctx.ecx);
				const int& iCurrentHeight = Memory::ReadRegister(ctx.edx);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
			});
		}

		std::uint8_t* CameraFOVInstructionScanResult = Memory::PatternScan(ExeModule(), "D9 01 A1");
		if (CameraFOVInstructionScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionScanResult, 2);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVInstructionScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionListUnlockHook{};
	SafetyHookMid m_resolutionListScrollHook{};
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	enum ResolutionInstructionsIndex
	{
		ResListUnlock,
		ResListScroll,
		ResWidthHeight
	};

	void CameraFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(ctx.ecx);

		m_newCameraFOV = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV, m_aspectRatioScale) * m_fovFactor;

		FPU::FLD(m_newCameraFOV);
	}

	inline static ApacheLongbowAssaultFix* s_instance_ = nullptr;
};

static std::unique_ptr<ApacheLongbowAssaultFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ApacheLongbowAssaultFix>(hModule);
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