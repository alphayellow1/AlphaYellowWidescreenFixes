#include "..\..\common\FixBase.hpp"

class ApacheAirAssaultFix final : public FixBase
{
public:
	explicit ApacheAirAssaultFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ApacheAirAssaultFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AH64ApacheAirAssaultWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "AH-64 Apache Air Assault";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Apache.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "6A ?? BB ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 6A ?? BB ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 6A ?? BB ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 6A ?? BB ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 35 ?? ?? ?? ?? 55",
		"8B 46 ?? 83 EC ?? 85 C0", "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? EB");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Scroll Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResListScroll] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

			m_resolutionListUnlockHook = safetyhook::create_mid(ResolutionScansResult[ResListUnlock], [](SafetyHookContext& ctx)
			{
				constexpr uintptr_t COMBO_PTR_ADDRESS = 0x00588A94;
				constexpr uintptr_t RETURN_ADDRESS = 0x00471748;
				constexpr uintptr_t SCAN_CMP_IMM_ADDRESS = 0x004717BC;
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

				void* combo = *reinterpret_cast<void**>(COMBO_PTR_ADDRESS);

				if (combo == nullptr)
				{
					s_resolution_count = 0;
					ctx.eip = RETURN_ADDRESS;
					return;
				}

				if (!s_item_buffer)
				{
					s_item_buffer = VirtualAlloc(nullptr, ITEM_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
				}

				if (!s_item_buffer)
				{
					s_resolution_count = 0;
					ctx.eip = RETURN_ADDRESS;
					return;
				}

				std::memset(s_item_buffer, 0, ITEM_BUFFER_SIZE);

				const uintptr_t combo_addr = reinterpret_cast<uintptr_t>(combo);

				// Apache.exe+6B920 uses [esi+10] as the item text buffer
				*reinterpret_cast<void**>(combo_addr + 0x10) = s_item_buffer;

				std::vector<Mode> modes;
				modes.reserve(128);

				DEVMODEA dm{};
				DWORD mode_index = 0;

				while (static_cast<int>(modes.size()) < MAX_MODES)
				{
					std::memset(&dm, 0, sizeof(dm));
					dm.dmSize = sizeof(dm);

					if (EnumDisplaySettingsA(nullptr, mode_index, &dm) == false)
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

					if (duplicate == true)
					{
						continue;
					}

					modes.push_back({dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel});
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

				int index = 0;

				for (const Mode& mode : modes)
				{
					if (index >= MAX_MODES)
					{
						break;
					}

					char text[64]{};

					// Matches the game's original parsing format, which is "%i x %i x %i"
					std::snprintf(text, sizeof(text), "%u x %u x %u", mode.width, mode.height, mode.bpp);

					uint8_t* item_base = *reinterpret_cast<uint8_t**>(combo_addr + 0x10);
					uint8_t* slot = item_base + index * ITEM_STRIDE;

					std::memset(slot, 0, ITEM_STRIDE);

					strncpy_s(reinterpret_cast<char*>(slot), ITEM_STRIDE, text, _TRUNCATE);

					++index;
				}

				// If EnumDisplaySettingsA somehow returns nothing, add the currently configured game resolution
				if (index == 0)
				{
					DWORD width = *reinterpret_cast<DWORD*>(0x0058B698);
					DWORD height = *reinterpret_cast<DWORD*>(0x0058B69C);
					DWORD bpp = *reinterpret_cast<DWORD*>(0x0058B6A0);

					char text[64]{};

					std::snprintf(text, sizeof(text), "%u x %u x %u", width, height, bpp);

					uint8_t* item_base = *reinterpret_cast<uint8_t**>(combo_addr + 0x10);
					uint8_t* slot = item_base;

					std::memset(slot, 0, ITEM_STRIDE);

					strncpy_s(reinterpret_cast<char*>(slot), ITEM_STRIDE, text, _TRUNCATE);

					index = 1;
				}

				s_resolution_count = index;

				*reinterpret_cast<int*>(combo_addr + 0x0C) = s_resolution_count;

				const int visible_rows = std::min(s_resolution_count, MAX_VISIBLE_ROWS);
				*reinterpret_cast<int*>(combo_addr + 0x1C) = visible_rows;

				// Preserve scroll/start index instead of resetting it every frame.
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

				*reinterpret_cast<int*>(combo_addr + 0x14) = old_scroll_start;

				const int row_height = *reinterpret_cast<int*>(combo_addr + 0x18);
				const int top_y = *reinterpret_cast<int*>(combo_addr + 0x28);
				*reinterpret_cast<int*>(combo_addr + 0x30) = top_y + row_height * visible_rows;

				*reinterpret_cast<int*>(combo_addr + 0x48) = -1;

				int* selected_index = reinterpret_cast<int*>(combo_addr + 0x4C);

				if (*selected_index >= s_resolution_count)
				{
					*selected_index = -1;
				}

				for (int i = 0; i < s_resolution_count; ++i)
				{
					const char* entry = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(s_item_buffer) + i * ITEM_STRIDE);
				}

				DWORD old_protect = 0;

				if (VirtualProtect(reinterpret_cast<void*>(SCAN_CMP_IMM_ADDRESS), 1, PAGE_EXECUTE_READWRITE, &old_protect))
				{
					*reinterpret_cast<uint8_t*>(SCAN_CMP_IMM_ADDRESS) = static_cast<uint8_t>(s_resolution_count);

					FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(SCAN_CMP_IMM_ADDRESS), 1);

					DWORD ignored = 0;

					VirtualProtect(reinterpret_cast<void*>(SCAN_CMP_IMM_ADDRESS), 1, old_protect, &ignored);
				}

				ctx.eip = RETURN_ADDRESS; // This skips the original hardcoded 12 resolution block
			});

			m_resolutionListScrollHook = safetyhook::create_mid(ResolutionScansResult[ResListScroll], [](SafetyHookContext& ctx)
			{
				constexpr uintptr_t COMBO_PTR_ADDR = 0x00588A94;

				void* resolution_combo = *reinterpret_cast<void**>(COMBO_PTR_ADDR);

				if (!resolution_combo)
				{
					return;
				}

				if (reinterpret_cast<void*>(ctx.esi) != resolution_combo)
				{
					return;
				}

				const uintptr_t combo_addr = reinterpret_cast<uintptr_t>(resolution_combo);

				int* total_count = reinterpret_cast<int*>(combo_addr + 0x0C);
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

				spdlog::info("Current resolution: {}x{}", iCurrentWidth, iCurrentHeight);
			});
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 01 D8 0D ?? ?? ?? ?? DF E0");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 2);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
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

	inline static ApacheAirAssaultFix* s_instance_ = nullptr;
};

static std::unique_ptr<ApacheAirAssaultFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ApacheAirAssaultFix>(hModule);
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