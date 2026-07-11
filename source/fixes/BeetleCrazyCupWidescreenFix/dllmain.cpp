#include "..\..\common\FixBase.hpp"

class BeetleCrazyCupFix final : public FixBase
{
public:
	explicit BeetleCrazyCupFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BeetleCrazyCupFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BeetleCrazyCupWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.4";
	}

	const char* TargetName() const override
	{
		return "Beetle Crazy Cup";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "beetle.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		FileIO::BinaryFile beetleCfgfile("Config/beetle.cfg", FileIO::OpenMode::ReadWrite);
		m_newResX = beetleCfgfile.Read<std::uint32_t>(0);
		m_newResY = beetleCfgfile.Read<std::uint32_t>(4);
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto MainMenuResolutionScansResult = Memory::PatternScan(ExeModule(), "C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 3B C6 89 3D",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? FF D7");
		if (Memory::AreAllSignaturesValid(MainMenuResolutionScansResult) == true)
		{
			spdlog::info("Main Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), MainMenuResolutionScansResult[MainMenuRes1] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), MainMenuResolutionScansResult[MainMenuRes2] - (std::uint8_t*)ExeModule());

			Memory::Write(MainMenuResolutionScansResult[MainMenuRes1] + 6, m_newResX);

			Memory::WriteNOPs(MainMenuResolutionScansResult[MainMenuRes1] + 12, 6);

			m_mainMenuResolutionHeight1Hook = safetyhook::create_mid(MainMenuResolutionScansResult[MainMenuRes1] + 12, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(0x01D3B168) = s_instance_->m_newResY;
			});

			Memory::WriteNOPs(MainMenuResolutionScansResult[MainMenuRes2], 20);

			m_mainMenuResolution2Hook = safetyhook::create_mid(MainMenuResolutionScansResult[MainMenuRes2], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(0x01D3B15C) = s_instance_->m_newResX;
				*reinterpret_cast<int*>(0x01D3B168) = s_instance_->m_newResY;
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? D9 15 ?? ?? ?? ?? D9 1D", "D9 81 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? D8 0D",
		"C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? C7 05 ?? ?? ?? ?? ?? ?? ?? ?? 89 3D", "D9 05 ?? ?? ?? ?? D8 0D ?? ?? ?? ?? 83 C4 ?? D8 0D");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Races Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[RacesFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Pre-races Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[PreRacesFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Main Menu Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[MainMenuFOV] - (std::uint8_t*)ExeModule());
			spdlog::info("Replay Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[ReplayFOV] - (std::uint8_t*)ExeModule());

			// Races Camera FOV
			m_racesFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[RacesFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[RacesFOV], 6);

			m_racesCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[RacesFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentRacesCameraFOV = Memory::ReadMem(s_instance_->m_racesFOVAddress);
				s_instance_->m_newRacesFOV = (fCurrentRacesCameraFOV / s_instance_->m_aspectRatioScale) / s_instance_->m_fovFactor;
				FPU::FMUL(s_instance_->m_newRacesFOV);
			});

			// Pre-races Camera FOV
			m_preRacesFOVOffset = Memory::GetPointerFromAddress(CameraFOVScansResult[PreRacesFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[PreRacesFOV], 6);

			m_preRacesCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[PreRacesFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentPreRacesCameraFOV = Memory::ReadMem(ctx.ecx + s_instance_->m_preRacesFOVOffset);
				s_instance_->m_newPreRacesFOV = fCurrentPreRacesCameraFOV / s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newPreRacesFOV);
			});

			// Main Menu Camera FOV
			m_newMainMenuFOV = 249.5998993f / m_aspectRatioScale;

			Memory::Write(CameraFOVScansResult[MainMenuFOV] + 6, m_newMainMenuFOV);

			// Replay Camera FOV
			m_replayFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[ReplayFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[ReplayFOV], 6);

			m_replayCameraFOVHook = safetyhook::create_mid(CameraFOVScansResult[ReplayFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentReplayCameraFOV = Memory::ReadMem(s_instance_->m_replayFOVAddress);
				s_instance_->m_newReplayFOV = fCurrentReplayCameraFOV / s_instance_->m_aspectRatioScale;
				FPU::FLD(s_instance_->m_newReplayFOV);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_mainMenuResolutionHeight1Hook{};
	SafetyHookMid m_mainMenuResolution2Hook{};	

	float m_newRacesFOV = 0.0f;
	float m_newPreRacesFOV = 0.0f;
	float m_newMainMenuFOV = 0.0f;
	float m_newReplayFOV = 0.0f;
	uintptr_t m_racesFOVAddress = 0;
	uintptr_t m_preRacesFOVOffset = 0;
	uintptr_t m_replayFOVAddress = 0;

	enum MainMenuResolutionsIndex
	{
		MainMenuRes1,
		MainMenuRes2
	};

	enum CameraFOVInstructionsIndex
	{
		RacesFOV,
		PreRacesFOV,
		MainMenuFOV,
		ReplayFOV
	};

	SafetyHookMid m_racesCameraFOVHook{};
	SafetyHookMid m_preRacesCameraFOVHook{};
	SafetyHookMid m_replayCameraFOVHook{};

	inline static BeetleCrazyCupFix* s_instance_ = nullptr;
};

static std::unique_ptr<BeetleCrazyCupFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BeetleCrazyCupFix>(hModule);
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