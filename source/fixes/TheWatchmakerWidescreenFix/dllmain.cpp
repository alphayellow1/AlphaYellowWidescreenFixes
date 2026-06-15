#include "..\..\common\FixBase.hpp"

class TheWatchmakerFix final : public FixBase
{
public:
	explicit TheWatchmakerFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TheWatchmakerFix() override
	{
		if (m_newModeTable)
		{
			VirtualFree(m_newModeTable, 0, MEM_RELEASE);
			m_newModeTable = nullptr;
		}

		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheWatchmakerWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "The Watchmaker";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Wm.exe") ||
		Util::stringcmp_caseless(exeName, "Setup.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_renderDllModule = Memory::GetHandle("render.dll");
		m_renderDllModuleName = Memory::GetModuleName(m_renderDllModule);

		auto ResolutionListUnlockScanResult = Memory::PatternScan(m_renderDllModule, "74 ?? 83 F8 ?? 75 ?? 8B 4E");
		if (ResolutionListUnlockScanResult)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", m_renderDllModuleName.c_str(), ResolutionListUnlockScanResult - (std::uint8_t*)m_renderDllModule);

			Memory::WriteNOPs(ResolutionListUnlockScanResult + 5, 2);
			Memory::WriteNOPs(ResolutionListUnlockScanResult + 16, 2);
			Memory::WriteNOPs(ResolutionListUnlockScanResult + 24, 2);
			Memory::WriteNOPs(ResolutionListUnlockScanResult + 33, 2);
		}
		else
		{
			spdlog::error("Failed to locate resolution list unlock scan memory address.");
			return;
		}

		m_modeCount = reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uint8_t*>(m_renderDllModule) + 0x99070);

		m_newModeTable = reinterpret_cast<std::uint8_t*>(VirtualAlloc(nullptr, kModeCapacity * kModeEntrySize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

		if (!m_newModeTable)
		{
			spdlog::error("Failed to allocate expanded resolution mode table.");
			return;
		}

		std::memset(m_newModeTable, 0, kModeCapacity * kModeEntrySize);

		PatchModeTableReferences();
		InstallModeLimitHook();

		if (Util::stringcmp_caseless(ExeName(), "Wm.exe"))
		{
			auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "89 0D ?? ?? ?? ?? 89 15 ?? ?? ?? ?? BF");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - reinterpret_cast<std::uint8_t*>(ExeModule()));

				m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
				{
					const int& iCurrentWidth = Memory::ReadRegister(ctx.ecx);
					const int& iCurrentHeight = Memory::ReadRegister(ctx.edx);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}

			auto CameraFOVScansResult = Memory::PatternScan(m_renderDllModule, "D9 44 24 ?? D8 0D", ExeModule(), "8B 41 ?? 50 A1 ?? ?? ?? ?? 50 56 99 2B C2 D1 F8 50 8B C6 99 2B C2 D1 F8 50 51 E8 ?? ?? ?? ?? 8B 0D",
			"8B 51 ?? 8B 35 ?? ?? ?? ?? 52 50 99 2B C2 56 D1 F8 50 8B C6 99 2B C2 D1 F8 50 51 E8 ?? ?? ?? ?? 66 A1", "8B 51 ?? 8B 35 ?? ?? ?? ?? 52 50 99 2B C2 56 D1 F8 50 8B C6 99 2B C2 D1 F8 50 51 E8 ?? ?? ?? ?? 6A");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("General FOV Instruction: Address is {:s}+{:x}", m_renderDllModuleName.c_str(), CameraFOVScansResult[General] - (std::uint8_t*)m_renderDllModule);
				spdlog::info("Gameplay FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay1] - (std::uint8_t*)ExeModule());
				spdlog::info("Gameplay FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay2] - (std::uint8_t*)ExeModule());
				spdlog::info("Gameplay FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay3] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(CameraFOVScansResult[General], 4);

				m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
				{
					s_instance_->GeneralFOVMidHook(ctx);
				});

				Memory::WriteNOPs(CameraFOVScansResult[Gameplay1], 3);

				m_gameplayFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay1], [](SafetyHookContext& ctx)
				{
					float& fCurrentGameplayFOV1 = Memory::ReadMem(ctx.ecx + 0x30);
					s_instance_->m_newGameplayFOV1 = fCurrentGameplayFOV1 * s_instance_->m_fovFactor;
					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newGameplayFOV1);
				});

				Memory::WriteNOPs(CameraFOVScansResult[Gameplay2], 3);

				m_gameplayFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay2], [](SafetyHookContext& ctx)
				{
					float& fCurrentGameplayFOV2 = Memory::ReadMem(ctx.ecx + 0x30);
					s_instance_->m_newGameplayFOV2 = fCurrentGameplayFOV2 * s_instance_->m_fovFactor;
					ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newGameplayFOV2);
				});

				Memory::WriteNOPs(CameraFOVScansResult[Gameplay3], 3);

				m_gameplayFOV3Hook = safetyhook::create_mid(CameraFOVScansResult[Gameplay3], [](SafetyHookContext& ctx)
				{
					float& fCurrentGameplayFOV3 = Memory::ReadMem(ctx.ecx + 0x30);
					s_instance_->m_newGameplayFOV3 = fCurrentGameplayFOV3 * s_instance_->m_fovFactor;
					ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newGameplayFOV3);
				});
			}
		}
	}

private:
	HMODULE m_renderDllModule = nullptr;
	std::string m_renderDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newGeneralFOV = 0.0f;
	float m_newGameplayFOV1 = 0.0f;
	float m_newGameplayFOV2 = 0.0f;
	float m_newGameplayFOV3 = 0.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_gameplayFOV1Hook{};
	SafetyHookMid m_gameplayFOV2Hook{};
	SafetyHookMid m_gameplayFOV3Hook{};

	enum CameraFOVInstructionsIndex
	{
		General,
		Gameplay1,
		Gameplay2,
		Gameplay3
	};

	static constexpr std::size_t kModeEntrySize = 0x8C;
	static constexpr std::size_t kModeCapacity = 4096;

	std::uint8_t* m_newModeTable = nullptr;
	std::uint32_t* m_modeCount = nullptr;

	SafetyHookMid m_modeLimitHook{};

	static IMAGE_SECTION_HEADER* FindSection(HMODULE module, const char* name)
	{
		auto* base = reinterpret_cast<std::uint8_t*>(module);
		auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
		auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		auto* section = IMAGE_FIRST_SECTION(nt);

		for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++)
		{
			char sectionName[9]{};
			std::memcpy(sectionName, section[i].Name, 8);

			if (std::strcmp(sectionName, name) == 0)
			{
				return &section[i];
			}
		}

		return nullptr;
	}

	void PatchModeTableReferences()
	{
		auto* text = FindSection(m_renderDllModule, ".text");
		if (!text)
		{
			spdlog::error("Failed to find .text section in render.dll.");
			return;
		}

		auto* moduleBase = reinterpret_cast<std::uint8_t*>(m_renderDllModule);
		auto* textStart = moduleBase + text->VirtualAddress;
		auto* textEnd = textStart + text->Misc.VirtualSize;

		const auto oldTable = reinterpret_cast<std::uintptr_t>(moduleBase + 0x9E590);
		const auto newTable = reinterpret_cast<std::uintptr_t>(m_newModeTable);

		const std::array<std::uint32_t, 5> oldRefs =
		{
			static_cast<std::uint32_t>(oldTable + 0x00),
			static_cast<std::uint32_t>(oldTable + 0x04),
			static_cast<std::uint32_t>(oldTable + 0x08),
			static_cast<std::uint32_t>(oldTable + 0x0C),
			static_cast<std::uint32_t>(oldTable + 0x10),
		};

		const std::array<std::uint32_t, 5> newRefs =
		{
			static_cast<std::uint32_t>(newTable + 0x00),
			static_cast<std::uint32_t>(newTable + 0x04),
			static_cast<std::uint32_t>(newTable + 0x08),
			static_cast<std::uint32_t>(newTable + 0x0C),
			static_cast<std::uint32_t>(newTable + 0x10),
		};

		std::size_t patchedRefs = 0;

		for (auto* p = textStart; p + sizeof(std::uint32_t) <= textEnd; p++)
		{
			std::uint32_t value{};
			std::memcpy(&value, p, sizeof(value));

			for (std::size_t i = 0; i < oldRefs.size(); i++)
			{
				if (value == oldRefs[i])
				{
					Memory::Write(p, newRefs[i]);
					patchedRefs++;
					break;
				}
			}
		}
	}

	void InstallModeLimitHook()
	{
		m_modeLimitHook = safetyhook::create_mid((uintptr_t)m_renderDllModule + 0x5378, [](SafetyHookContext& ctx)
		{
			const auto count = static_cast<std::uint32_t>(ctx.edx);

			constexpr std::uint32_t ZF = 0x40;

			if (count < kModeCapacity)
			{
				ctx.eflags &= ~ZF;
			}
			else
			{
				ctx.eflags |= ZF;
			}
		});
	}

	void GeneralFOVMidHook(SafetyHookContext& ctx)
	{
		float& fCurrentGeneralFOV = Memory::ReadMem(ctx.esp + 0x1C);
		m_newGeneralFOV = Maths::CalculateNewFOV_DegBased(fCurrentGeneralFOV, m_aspectRatioScale);
		FPU::FLD(m_newGeneralFOV);
	}

	inline static TheWatchmakerFix* s_instance_ = nullptr;
};

static std::unique_ptr<TheWatchmakerFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<TheWatchmakerFix>(hModule);
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