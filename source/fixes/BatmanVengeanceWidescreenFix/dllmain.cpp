#include "..\..\common\FixBase.hpp"

class BatmanVengeanceFix final : public FixBase
{
public:
	explicit BatmanVengeanceFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~BatmanVengeanceFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "BatmanVengeanceWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.7";
	}

	const char* TargetName() const override
	{
		return "Batman: Vengeance";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Batman.exe");
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
		m_osrDllModule = Memory::GetHandle("osr_dx8_vf.dll", 50);
		m_osrDllModuleName = Memory::GetModuleName(m_osrDllModule);

		auto ResolutionListScanResult = Memory::PatternScan(m_osrDllModule, "00 00 00 00 02 00 00 80 01 00 00 80 02 00 00 E0 01 00 00 20 03 00 00 58 02 00 00 00 04 00 00 00 03 00 00 00 05 00 00 00 04 00 00 40 06 00 00 B0 04 00 00 3C 00 00 00");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", m_osrDllModuleName.c_str(), ResolutionListScanResult - (std::uint8_t*)m_osrDllModule);

			// 640x480
			Memory::Write(ResolutionListScanResult + 11, m_newResX);
			Memory::Write(ResolutionListScanResult + 15, m_newResY);
			// 800x600
			Memory::Write(ResolutionListScanResult + 19, m_newResX);
			Memory::Write(ResolutionListScanResult + 23, m_newResY);
			// 1024x768
			Memory::Write(ResolutionListScanResult + 27, m_newResX);
			Memory::Write(ResolutionListScanResult + 31, m_newResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		m_enginecoreDllModule = Memory::GetHandle("enginecore_vf.dll", 50);
		m_aisdkDllModule = Memory::GetHandle("aisdk_gamedll_vf.dll", 50);
		m_dlgDllModule = Memory::GetHandle("DLG_vf.dll", 50);
		m_enginecoreDllModuleName = Memory::GetModuleName(m_enginecoreDllModule);
		m_aisdkDllModuleName = Memory::GetModuleName(m_aisdkDllModule);
		m_dlgDllModuleName = Memory::GetModuleName(m_dlgDllModule);

		auto AspectRatioScanResult = Memory::PatternScan(m_dlgDllModule, "8B 46 ?? 8B 4E ?? 8B 96 ?? ?? ?? ?? 53");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", m_dlgDllModuleName.c_str(), AspectRatioScanResult - (std::uint8_t*)m_dlgDllModule);

			Memory::WriteNOPs(AspectRatioScanResult, 3);

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentResX = Memory::ReadMem(ctx.esi + 0x24);
				s_instance_->m_newAspectRatio2 = fCurrentResX * s_instance_->m_aspectRatioScale;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio2);
			});
		}
		else
		{
			spdlog::info("Cannot locate the aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(m_enginecoreDllModule, "D8 4C 24 ?? D9 5C 24 ?? D9 44 24 ?? D8 74 24", m_aisdkDllModule, "8B 0D ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 51 52 FF 15 ?? ?? ?? ?? 8B 0D",
		"8B 8E ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 51 52 FF 15 ?? ?? ?? ?? 8B 8E", "68 ?? ?? ?? ?? 52 FF 15 ?? ?? ?? ?? A1", "68 ?? ?? ?? ?? 50 FF 15 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 89 8E",
		"8B 0D ?? ?? ?? ?? 8B 96 ?? ?? ?? ?? 55");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("General Camera FOV Instruction: Address is {:s}+{:x}", m_enginecoreDllModuleName.c_str(), CameraFOVScansResult[GeneralFOV] - (std::uint8_t*)m_enginecoreDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 1: Address is {:s}+{:x}", m_aisdkDllModuleName.c_str(), CameraFOVScansResult[GameplayFOV1] - (std::uint8_t*)m_aisdkDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 2: Address is {:s}+{:x}", m_aisdkDllModuleName.c_str(), CameraFOVScansResult[GameplayFOV2] - (std::uint8_t*)m_aisdkDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 3: Address is {:s}+{:x}", m_aisdkDllModuleName.c_str(), CameraFOVScansResult[GameplayFOV3] - (std::uint8_t*)m_aisdkDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 4: Address is {:s}+{:x}", m_aisdkDllModuleName.c_str(), CameraFOVScansResult[GameplayFOV4] - (std::uint8_t*)m_aisdkDllModule);
			spdlog::info("Gameplay Camera FOV Instruction 5: Address is {:s}+{:x}", m_aisdkDllModuleName.c_str(), CameraFOVScansResult[GameplayFOV5] - (std::uint8_t*)m_aisdkDllModule);

			Memory::WriteNOPs(CameraFOVScansResult[GeneralFOV], 4);

			// Instruction is located in the CAM_vAdjustCameraToViewport function
			m_generalFOVHook = safetyhook::create_mid(CameraFOVScansResult[GeneralFOV], [](SafetyHookContext& ctx)
			{
				float& fCurrentGeneralFOV = Memory::ReadMem(ctx.esp + 0xC);
				s_instance_->m_newGeneralFOV = fCurrentGeneralFOV * s_instance_->m_aspectRatioScale;
				FPU::FMUL(s_instance_->m_newGeneralFOV);
			});

			m_gameplayFOVAddress1 = Memory::GetPointerFromAddress(CameraFOVScansResult[GameplayFOV1] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[GameplayFOV1], 6);

			m_gameplayFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[GameplayFOV1], [](SafetyHookContext& ctx)
			{
				s_instance_->GameplayFOVMidHook(s_instance_->m_gameplayFOVAddress1, ctx.ecx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[GameplayFOV2], 6);

			m_gameplayFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[GameplayFOV2], [](SafetyHookContext& ctx)
			{
				s_instance_->GameplayFOVMidHook(ctx.esi + 0x13C, ctx.ecx);
			});

			m_newCameraFOV3 = 1.299999952f * m_fovFactor;

			Memory::Write(CameraFOVScansResult[GameplayFOV3] + 1, m_newCameraFOV3);
			Memory::Write(CameraFOVScansResult[GameplayFOV4] + 1, m_newCameraFOV3);

			m_gameplayFOVAddress5 = Memory::GetPointerFromAddress(CameraFOVScansResult[GameplayFOV5] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[GameplayFOV5], 6);

			m_gameplayFOV5Hook = safetyhook::create_mid(CameraFOVScansResult[GameplayFOV5], [](SafetyHookContext& ctx)
			{
				s_instance_->GameplayFOVMidHook(s_instance_->m_gameplayFOVAddress5, ctx.ecx);
			});
		}
	}

private:
	HMODULE m_osrDllModule = nullptr;
	HMODULE m_enginecoreDllModule = nullptr;
	HMODULE m_dlgDllModule = nullptr;
	HMODULE m_aisdkDllModule = nullptr;	
	std::string m_osrDllModuleName = "";
	std::string m_enginecoreDllModuleName = "";
	std::string m_dlgDllModuleName = "";
	std::string m_aisdkDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	float m_newGeneralFOV = 0.0f;
	float m_newGameplayFOV = 0.0f;
	float m_newCameraFOV3 = 0.0f;
	uintptr_t m_gameplayFOVAddress1 = 0;
	uintptr_t m_gameplayFOVAddress5 = 0;
	
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_generalFOVHook{};
	SafetyHookMid m_gameplayFOV1Hook{};
	SafetyHookMid m_gameplayFOV2Hook{};
	SafetyHookMid m_gameplayFOV5Hook{};

	enum CameraFOVInstructionsIndices
	{
		GeneralFOV,
		GameplayFOV1,
		GameplayFOV2,
		GameplayFOV3,
		GameplayFOV4,
		GameplayFOV5,
		GameplayFOV6
	};

	void GameplayFOVMidHook(uintptr_t FOVAddress, uintptr_t& destInstruction)
	{
		float& fCurrentGameplayFOV = Memory::ReadMem(FOVAddress);
		m_newGameplayFOV = fCurrentGameplayFOV * m_fovFactor;
		destInstruction = std::bit_cast<uintptr_t>(m_newGameplayFOV);
	}

	inline static BatmanVengeanceFix* s_instance_ = nullptr;
};

static std::unique_ptr<BatmanVengeanceFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<BatmanVengeanceFix>(hModule);
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