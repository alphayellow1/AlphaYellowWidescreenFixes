#include "..\..\common\FixBase.hpp"

class BarbieHorseAdventuresMysteryRideFix final : public FixBase
{
public:
	explicit BarbieHorseAdventuresMysteryRideFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BarbieHorseAdventuresMysteryRideFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BarbieHorseAdventuresMysteryRideWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Barbie Horse Adventures: Mystery Ride";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Barbie Horse.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "8B 02 A3 ?? ?? ?? ?? 8B 42");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			Memory::PatchBytes((uint8_t*)ExeModule() + 0x13FF78, "\xE9\x80\x01\x00\x00\x90");
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x140106, 8);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x140455, 6);
			Memory::WriteNOPs((uint8_t*)ExeModule() + 0x140455 + 13, 6);

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.edx);
				int& iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;
				s_instance_->WriteARAndFOV();
				s_instance_->m_resolutionHook.disable();
			});
		}

		AspectRatioScansResult = Memory::PatternScan(ExeModule(), "75 00 A3 F0 A8 76 00 68 AB AA AA 3F", "8A F7 FF 83 C4 0C 68 AB AA AA 3F",
		"26 F7 FF 83 C4 0C 68 AB AA AA 3F 68", "A3 C4 31 77 00 68 AB AA AA 3F 68", "D8 E9 98 00 00 00 68 AB AA AA 3F 68", "04 A3 F0 A8 76 00 68 AB AA AA 3F 68",
		"ED FF 83 C4 04 89 45 F8 68 AB AA AA 3F 68", "15 F0 A8 76 00 89 15 C4 31 77 00 68 AB AA AA 3F", "EC FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68",
		"04 8B 0D F0 A8 76 00 89 0D C4 31 77 00 68 AB AA AA 3F 68", "D6 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68",
		"00 51 E8 58 B5 EB FF 83 C4 04 A3 C4 31 77 00 8B 15 C4 31 77 00 89 15 F0 A8 76 00 68 AB AA AA 3F 68", "0D F0 A8 76 00 68 AB AA AA 3F", "00 52 E8 9F 5C EB FF 83 C4 04 A3 C4 31 77 00 A1 C4 31 77 00 A3 F0 A8 76 00 68 AB AA AA 3F 68");
		if (Memory::AreAllSignaturesValid(AspectRatioScansResult) == true)
		{
			spdlog::info("Aspect Ratio Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR1] + 7 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR2] + 6 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR3] + 6 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR4] + 5 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR5] + 6 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR6] + 6 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR7] + 8 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR8] + 11 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR9] + 22 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 10: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR10] + 13 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 11: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR11] + 23 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 12: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR12] + 27 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 13: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR13] + 5 - (std::uint8_t*)ExeModule());
			spdlog::info("Aspect Ratio Instruction 14: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScansResult[AR14] + 25 - (std::uint8_t*)ExeModule());			
		}

		CameraFOVScansResult = Memory::PatternScan(ExeModule(), "68 00 00 00 3F 8B 0D 48", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 B5",
		"68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 7E", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 CE", "68 00 00 00 3F 8B 45",
		"68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 8A", "68 00 00 00 3F A1 C8 31 77 00 50 8B 4D", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D C4",
		"68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 2A", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 97", "68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 0E",
		"68 00 00 00 3F A1 C8 31 77 00 50 8B 0D F0 A8 76 00 51 E8 D2", "68 00 00 00 3F 8B 15 C8 31 77 00 52 A1 F0 A8 76 00 50 E8 6B", "68 00 00 00 3F 8B 0D C8 31 77 00 51 8B 15 F0 A8 76 00 52 E8 1A");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 3: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV3] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 4: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV4] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 5: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV5] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 6: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV6] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 7: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV7] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 8: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV8] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 9: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV9] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 10: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV10] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 11: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV11] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 12: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV12] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 13: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV13] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 14: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[FOV14] - (std::uint8_t*)ExeModule());			
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_originalCameraFOV = 0.5f;

	SafetyHookMid m_resolutionHook{};

	enum AspectRatioInstructionsIndices
	{
		AR1,
		AR2,
		AR3,
		AR4,
		AR5,
		AR6,
		AR7,
		AR8,
		AR9,
		AR10,
		AR11,
		AR12,
		AR13,
		AR14
	};

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
		FOV9,
		FOV10,
		FOV11,
		FOV12,
		FOV13,
		FOV14
	};

	std::vector<std::uint8_t*> AspectRatioScansResult;
	std::vector<std::uint8_t*> CameraFOVScansResult;

	void WriteARAndFOV()
	{
		Memory::Write(AspectRatioScansResult[AR1] + 8, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR2] + 7, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR3] + 7, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR4] + 6, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR5] + 7, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR6] + 7, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR7] + 9, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR8] + 12, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR9] + 23, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR10] + 14, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR11] + 24, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR12] + 28, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR13] + 6, m_newAspectRatio);
		Memory::Write(AspectRatioScansResult[AR14] + 26, m_newAspectRatio);

		m_newCameraFOV = m_originalCameraFOV * m_aspectRatioScale * m_fovFactor;

		Memory::Write(CameraFOVScansResult, FOV1, FOV14, 1, m_newCameraFOV);
	}

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
	};

	inline static BarbieHorseAdventuresMysteryRideFix* s_instance_ = nullptr;
};

static std::unique_ptr<BarbieHorseAdventuresMysteryRideFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BarbieHorseAdventuresMysteryRideFix>(hModule);
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