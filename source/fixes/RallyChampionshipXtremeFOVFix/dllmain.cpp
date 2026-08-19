#include "..\..\common\FixBase.hpp"

class RallyChampionshipXtremeFix final : public FixBase
{
public:
	explicit RallyChampionshipXtremeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RallyChampionshipXtremeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "RallyChampionshipXtremeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.0";
	}

	const char* TargetName() const override
	{
		return "Rally Championship Xtreme";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Rally.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "SkipIntroVideos", m_skipIntroVideos);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_skipIntroVideos);
	}

	void ApplyFix() override
	{
		FileIO::BinaryFile xtremeCfgFile("Data/xtreme.cfg", FileIO::OpenMode::Read);
		m_newResX = xtremeCfgFile.Read<int>(8);
		m_newResY = xtremeCfgFile.Read<int>(12);
		CalculateAR();

		m_resolutionScansResult = Memory::PatternScan(ExeModule(), "BA ?? ?? ?? ?? 8B 48 ?? 51", "8B 0D ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 89 0D");
		if (Memory::AreAllSignaturesValid(m_resolutionScansResult) == true)
		{
			spdlog::info("Main Menu Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[MainMenu] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), m_resolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::Write(m_resolutionScansResult[MainMenu] + 1, m_newResX);
			Memory::Write(m_resolutionScansResult[MainMenu] + 16, m_newResY);

			m_widthAddress = Memory::GetPointerFromAddress(m_resolutionScansResult[WidthHeight] + 2, Memory::PointerMode::Absolute);
			m_heightAddress = Memory::GetPointerFromAddress(m_resolutionScansResult[WidthHeight] + 8, Memory::PointerMode::Absolute);

			m_resolutionHook = safetyhook::create_mid(m_resolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				s_instance_->m_newResX = Memory::ReadMem(s_instance_->m_widthAddress);
				s_instance_->m_newResY = Memory::ReadMem(s_instance_->m_heightAddress);
				s_instance_->CalculateAR();
			});
		}

		m_aspectRatioScanResult = Memory::PatternScan(ExeModule(), "D8 0D ?? ?? ?? ?? 53 8B 5C 24");
		if (m_aspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_aspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_aspectRatioScanResult, 6);

			m_aspectRatioHook = safetyhook::create_mid(m_aspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				FPU::FMUL(s_instance_->m_newAspectRatio);
			});
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		m_cameraFOVScansResult = Memory::PatternScan(ExeModule(), "8B 8F ?? ?? ?? ?? D9 1C 24", "8B 46 ?? 8B 0E 8D 56");
		if (Memory::AreAllSignaturesValid(m_cameraFOVScansResult) == true)
		{
			spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV1] - (std::uint8_t*)ExeModule());
			spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", ExeName().c_str(), m_cameraFOVScansResult[FOV2] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(m_cameraFOVScansResult[FOV1], 6);
			Memory::WriteNOPs(m_cameraFOVScansResult[FOV2], 3);

			m_cameraFOV1Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(ctx.edi + 0x4B7FAC, ctx.ecx);
			});

			m_cameraFOV2Hook = safetyhook::create_mid(m_cameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVsMidHook(ctx.esi + 0x44, ctx.eax);
			});
		}

		if (m_skipIntroVideos == true)
		{
			m_skipIntroVideoScanResult = Memory::PatternScan(ExeModule(), "56 57 8B D3 B9");
			if (m_skipIntroVideoScanResult)
			{
				spdlog::info("Skip Intro Video Instruction: Address is {:s}+{:x}", ExeName().c_str(), m_skipIntroVideoScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(m_skipIntroVideoScanResult, "\xEB\x17");
			}
			else
			{
				spdlog::error("Failed to locate skip intro video instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	std::vector<std::uint8_t*> m_resolutionScansResult{};
	std::uint8_t* m_aspectRatioScanResult = nullptr;
	std::vector<std::uint8_t*> m_cameraFOVScansResult{};
	std::uint8_t* m_hudScanResult = nullptr;
	std::uint8_t* m_skipIntroVideoScanResult = nullptr;

	bool m_skipIntroVideos = false;

	float m_currentCameraFOV = 0.0f;

	uintptr_t m_widthAddress = 0;
	uintptr_t m_heightAddress = 0;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	void CameraFOVsMidHook(uintptr_t sourceFOVAddress, uintptr_t& destFOVAddress)
	{
		m_currentCameraFOV = Memory::ReadMem(sourceFOVAddress);
		m_newCameraFOV = m_currentCameraFOV * m_fovFactor;
		destFOVAddress = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	void CalculateAR()
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
	}

	enum ResolutionInstructionsIndices
	{
		MainMenu,
		WidthHeight
	};

	enum CameraFOVInstructionsIndices
	{
		FOV1,
		FOV2
	};

	inline static RallyChampionshipXtremeFix* s_instance_ = nullptr;
};

static std::unique_ptr<RallyChampionshipXtremeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<RallyChampionshipXtremeFix>(hModule);
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