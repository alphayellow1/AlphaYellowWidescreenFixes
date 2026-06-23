#include "..\..\common\FixBase.hpp"

class WinxClubFix final : public FixBase
{
public:
	explicit WinxClubFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~WinxClubFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "WinxClubWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Winx Club";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "WinxClub.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		s_instance_->m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / m_oldAspectRatio;

		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? EB ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? EB ?? C7 44 24 ?? ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? EB",
		"C7 06 ?? ?? ?? ?? C7 46 ?? ?? ?? ?? ?? 89 46 ?? 89 46 ?? A1", "A1 ?? ?? ?? ?? 89 02 8B 0D ?? ?? ?? ?? 89 4A ?? 66 A1");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResList] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[MainMenuRes] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Strings Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResStrings] - (std::uint8_t*)ExeModule());

			// 800x600
			Memory::Write(ResolutionScansResult[ResList] + 4, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 12, m_newResY);

			// 1024x768
			Memory::Write(ResolutionScansResult[ResList] + 22, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 30, m_newResY);

			// 1280x960
			Memory::Write(ResolutionScansResult[ResList] + 40, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 48, m_newResY);

			// 1600x1200
			Memory::Write(ResolutionScansResult[ResList] + 58, m_newResX);
			Memory::Write(ResolutionScansResult[ResList] + 66, m_newResY);

			// Main Menu Resolution (1024x768)
			Memory::Write(ResolutionScansResult[MainMenuRes] + 2, m_newResX);
			Memory::Write(ResolutionScansResult[MainMenuRes] + 9, m_newResY);
			Memory::Write(ResolutionScansResult[MainMenuRes] + 52, m_newResX);
			Memory::Write(ResolutionScansResult[MainMenuRes] + 59, m_newResY);

			// Resolution Strings
			m_newResolutionString = std::to_string(m_newResX) + " X " + std::to_string(m_newResY);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[ResStrings] + 142, [](SafetyHookContext& ctx)
			{
				s_instance_->m_string_800_600 = Memory::ReadMem(ctx.esi + 0x290);
				s_instance_->m_string_1024_768 = Memory::ReadMem(ctx.esi + 0x294);
				s_instance_->m_string_1280_960 = Memory::ReadMem(ctx.esi + 0x298);

				if (s_instance_->m_string_800_600)
				{
					std::snprintf(s_instance_->m_string_800_600, 15, "%u X %u", s_instance_->m_newResX, s_instance_->m_newResY);
				}

				if (s_instance_->m_string_1024_768)
				{
					std::snprintf(s_instance_->m_string_1024_768, 15, "%u X %u", s_instance_->m_newResX, s_instance_->m_newResY);
				}

				if (s_instance_->m_string_1280_960)
				{
					std::snprintf(s_instance_->m_string_1280_960, 15, "%u X %u", s_instance_->m_newResX, s_instance_->m_newResY);
				}
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D9 44 24 ?? 8B 44 24 ?? D8 0D ?? ?? ?? ?? 89 81", "8B 91 88 01 00 00 52 E8 ?? ?? ?? ??");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[General] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Gameplay] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScansResult[General], 4);

			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[General], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGeneralFOV = Memory::ReadMem(ctx.esp + 0x4);
				spdlog::info("Current general FOV: {:.12f}", s_instance_->m_currentGeneralFOV);
				s_instance_->m_newGeneralFOV = Maths::CalculateNewFOV_RadBased(s_instance_->m_currentGeneralFOV, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newGeneralFOV);
			});

			Memory::WriteNOPs(CameraFOVScansResult[Gameplay], 6);

			m_gameplayFOVHook = safetyhook::create_mid(CameraFOVScansResult[Gameplay], [](SafetyHookContext& ctx)
			{
				s_instance_->m_currentGameplayFOV = Memory::ReadMem(ctx.ecx + 0x188);

				spdlog::info("Current gameplay FOV: {:.12f}", s_instance_->m_currentGameplayFOV);

				if (s_instance_->m_currentGameplayFOV != s_instance_->m_newGameplayFOV)
				{
					s_instance_->m_newGameplayFOV = s_instance_->m_currentGameplayFOV * s_instance_->m_fovFactor;
				}

				ctx.edx = std::bit_cast<uintptr_t>(s_instance_->m_newGameplayFOV);
			});
		}

		auto WindowNameScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 56 FF 15 ?? ?? ?? ?? 83 C4 ?? 8B C6");
		if (WindowNameScanResult)
		{
			spdlog::info("Window Name Instruction: Address is {:s}+{:x}", ExeName().c_str(), WindowNameScanResult - (std::uint8_t*)ExeModule());

			m_newWindowName = "Winx Club";

			Memory::Write(WindowNameScanResult + 1, m_newWindowName);
		}
		else
		{
			spdlog::error("Failed to locate window name instruction memory address.");
			return;
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_gameplayFOVHook{};

	std::string m_newResolutionString = "";
	std::string m_newResolutionWidthString = "";
	const char* m_cNewResolutionString = "";
	const char* m_cNewResolutionWidthString = "";
	const char* m_newWindowName = "";

	char* m_string_800_600;
	char* m_string_1024_768;
	char* m_string_1280_960;

	float m_currentGeneralFOV = 0.0f;
	float m_currentGameplayFOV = 0.0f;
	float m_newGeneralFOV = 0.0f;
	float m_newGameplayFOV = 0.0f;

	enum ResolutionInstructionsIndices
	{
		ResList,
		MainMenuRes,
		ResStrings
	};

	enum CameraFOVInstructionsIndices
	{
		General,
		Gameplay
	};

	inline static WinxClubFix* s_instance_ = nullptr;
};

static std::unique_ptr<WinxClubFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<WinxClubFix>(hModule);
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