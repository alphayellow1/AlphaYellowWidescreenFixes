#include "..\..\common\FixBase.hpp"

class HitmanCodename47Fix final : public FixBase
{
public:
	explicit HitmanCodename47Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~HitmanCodename47Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "HitmanCodename47WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2.1";
	}

	const char* TargetName() const override
	{
		return "Hitman: Codename 47";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "Hitman.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);
		inipp::get_value(ini.sections["Settings"], "DrawDistanceFactor", m_drawDistanceFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_zoomFactor);
		spdlog_confparse(m_drawDistanceFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		m_rendererDllModule = Memory::GetHandle({ "RenderD3D.dll", "RenderOpenGL.dll" }, 20);
		m_rendererDllModuleName = Memory::GetModuleName(m_rendererDllModule);		

		auto ResolutionScanResult = Memory::PatternScan(m_rendererDllModule, "0F 84 ?? ?? ?? ?? 8B 42");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", m_rendererDllModuleName.c_str(), ResolutionScanResult - (std::uint8_t*)m_rendererDllModule);

			Memory::WriteNOPs(ResolutionScanResult, 6);
			Memory::WriteNOPs(ResolutionScanResult + 9, 7);

			Memory::Write(ResolutionScanResult + 17, m_newResX);
			Memory::Write(ResolutionScanResult + 22, m_newResY);
		}
		else
		{
			spdlog::error("Failed to locate resolution instructions scan memory address.");
			return;
		}

		m_hitmanDlcDllModule = Memory::GetHandle("HitmanDlc.dlc", 20);
		m_hitmanDlcDllModuleName = Memory::GetModuleName(m_hitmanDlcDllModule);

		auto ResolutionListScanResult = Memory::PatternScan(m_hitmanDlcDllModule, "c7 83 ?? ?? ?? ?? ?? ?? ?? ?? c7 83 ?? ?? ?? ?? ?? ?? ?? ?? eb ?? c7 83 ?? ?? ?? ?? ?? ?? ?? ?? c7 83 ?? ?? ?? ?? ?? ?? ?? ?? eb ?? c7 83 ?? ?? ?? ?? ?? ?? ?? ?? c7 83 ?? ?? ?? ?? ?? ?? ?? ?? eb ?? c7 83 ?? ?? ?? ?? ?? ?? ?? ?? c7 83 ?? ?? ?? ?? ?? ?? ?? ?? eb");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), ResolutionListScanResult - (std::uint8_t*)m_hitmanDlcDllModule);

			// Resolution List
			// 640x480
			Memory::Write(ResolutionListScanResult + 6, m_newResX);
			Memory::Write(ResolutionListScanResult + 16, m_newResX);
			// 800x600
			Memory::Write(ResolutionListScanResult + 28, m_newResX);
			Memory::Write(ResolutionListScanResult + 38, m_newResX);
			// 1024x768
			Memory::Write(ResolutionListScanResult + 50, m_newResX);
			Memory::Write(ResolutionListScanResult + 60, m_newResX);
			// 1200x1024
			Memory::Write(ResolutionListScanResult + 72, m_newResX);
			Memory::Write(ResolutionListScanResult + 82, m_newResX);
			// 1600x1200
			Memory::Write(ResolutionListScanResult + 94, m_newResX);
			Memory::Write(ResolutionListScanResult + 104, m_newResX);
		}
		else
		{
			spdlog::error("Failed to locate resolution list scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(m_hitmanDlcDllModule, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF FF 90 ?? ?? ?? ?? 5F 89 5E",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF FF 90 ?? ?? ?? ?? 5F 5E 8B E5", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF FF 92 ?? ?? ?? ?? DD 44 24",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF FF 90 ?? ?? ?? ?? 5F 5E 5B 8B E5 5D C2 ?? ?? 90 90 90 90 90 90 90 55", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CE FF 90 ?? ?? ?? ?? 5F 5E 8B E5",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8B CF FF 90 ?? ?? ?? ?? 5F 5E 5B 8B E5 5D C2 ?? ?? 90 90 90 90 90 90 90 90", "DD 44 24 ?? DC 0D ?? ?? ?? ?? 6A", "DC 0D ?? ?? ?? ?? 8B 01",
		"D9 84 86 ?? ?? ?? ?? 8B 4E ?? DD 99 ?? ?? ?? ?? 8B 46 ?? 83 88", "D9 84 86 ?? ?? ?? ?? 8B 4E ?? DD 99 ?? ?? ?? ?? 8B 46 ?? 8B 88");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Character FOV Instruction: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[MouseConCam] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Base Camera FOV Instruction: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[BaseCam] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Main Camera FOV Instruction: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[MainCam] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("New Dialog Camera FOV Instruction: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[NewDialogCam] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Plane Camera FOV Instruction: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[PlaneCam] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Static Activation Camera FOV Instruction: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[StaticActivationCam] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Cutscenes FOV Instruction 1: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[CutscenesCam1] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Cutscenes FOV Instruction 2: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[CutscenesCam2] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Sniper Scope FOV Instruction 1: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[ScopeCam1] - (std::uint8_t*)m_hitmanDlcDllModule);
			spdlog::info("Sniper Scope FOV Instruction 2: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), CameraFOVScansResult[ScopeCam2] - (std::uint8_t*)m_hitmanDlcDllModule);

			m_newCameraFOV1 = Maths::CalculateNewFOV_DegBased(m_originalCameraFOV, m_aspectRatioScale);
			m_newCameraFOV2 = m_newCameraFOV1 * static_cast<double>(m_fovFactor);

			m_cameraFOV1Bits = std::bit_cast<std::uint64_t>(m_newCameraFOV1);
			m_cameraFOV2Bits = std::bit_cast<std::uint64_t>(m_newCameraFOV2);

			m_cameraFOV1LowerBits = static_cast<std::uint32_t>(m_cameraFOV1Bits & 0xFFFFFFFFull);
			m_cameraFOV1HigherBits = static_cast<std::uint32_t>((m_cameraFOV1Bits >> 32) & 0xFFFFFFFFull);

			m_cameraFOV2LowerBits = static_cast<std::uint32_t>(m_cameraFOV2Bits & 0xFFFFFFFFull);
			m_cameraFOV2HigherBits = static_cast<std::uint32_t>((m_cameraFOV2Bits >> 32) & 0xFFFFFFFFull);

			Memory::Write(CameraFOVScansResult[MouseConCam] + 1, m_cameraFOV2HigherBits);
			Memory::Write(CameraFOVScansResult[MouseConCam] + 6, m_cameraFOV2LowerBits);

			Memory::Write(CameraFOVScansResult, BaseCam, StaticActivationCam, 1, m_cameraFOV1HigherBits);
			Memory::Write(CameraFOVScansResult, BaseCam, StaticActivationCam, 6, m_cameraFOV1LowerBits);

			Memory::WriteNOPs(CameraFOVScansResult[CutscenesCam1], 4);

			m_cutscenesFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[CutscenesCam1], [](SafetyHookContext& ctx)
			{
				double& dCurrentCutscenesFOV1 = Memory::ReadMem(ctx.esp + 0x14);
				s_instance_->m_newCutscenesFOV1 = Maths::CalculateNewFOV_DegBased(dCurrentCutscenesFOV1, s_instance_->m_aspectRatioScale);
				FPU::FLD(s_instance_->m_newCutscenesFOV1);
			});

			m_newCutscenesFOV2 = Maths::CalculateNewFOV_DegBased(m_originalCutscenesFOV2, s_instance_->m_aspectRatioScale);

			Memory::Write(CameraFOVScansResult[CutscenesCam2] + 2, &m_newCutscenesFOV2);

			Memory::WriteNOPs(CameraFOVScansResult[ScopeCam1], 7);

			m_scopeViewFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[ScopeCam1], [](SafetyHookContext& ctx)
			{
				float& m_currentScopeViewFOV1 = Memory::ReadMem(ctx.esi + ctx.eax * 0x4 + 0xD8);
				s_instance_->m_newScopeViewFOV1 = Maths::CalculateNewFOV_RadBased(m_currentScopeViewFOV1, s_instance_->m_aspectRatioScale) / s_instance_->m_zoomFactor;
				FPU::FLD(s_instance_->m_newScopeViewFOV1);
			});

			Memory::WriteNOPs(CameraFOVScansResult[ScopeCam2], 7);

			m_scopeViewFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[ScopeCam2], [](SafetyHookContext& ctx)
			{
				float& m_currentScopeViewFOV2 = Memory::ReadMem(ctx.esi + ctx.eax * 0x4 + 0xD8);
				s_instance_->m_newScopeViewFOV2 = Maths::CalculateNewFOV_RadBased(m_currentScopeViewFOV2, s_instance_->m_aspectRatioScale) / s_instance_->m_zoomFactor;
				FPU::FLD(s_instance_->m_newScopeViewFOV2);
			});
		}

		auto DrawDistanceScanResult = Memory::PatternScan(m_hitmanDlcDllModule, "8B 85 ?? ?? ?? ?? 8B 4B ?? 8B 11 50 8B 85 ?? ?? ?? ?? 50 FF 92 ?? ?? ?? ?? 8B 43");
		if (DrawDistanceScanResult)
		{
			spdlog::info("Draw Distance Instruction: Address is {:s}+{:x}", m_hitmanDlcDllModuleName.c_str(), DrawDistanceScanResult - (std::uint8_t*)m_hitmanDlcDllModule);

			Memory::WriteNOPs(DrawDistanceScanResult, 6);

			m_drawDistanceHook = safetyhook::create_mid(DrawDistanceScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentDrawDistance = Memory::ReadMem(ctx.ebp + 0x18A);
				s_instance_->m_newDrawDistance = fCurrentDrawDistance * s_instance_->m_drawDistanceFactor;
				ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newDrawDistance);
			});
		}
		else
		{
			spdlog::error("Failed to locate draw distance instruction memory address.");
			return;
		}
	}

private:
	HMODULE m_rendererDllModule = nullptr;
	HMODULE m_hitmanDlcDllModule = nullptr;
	std::string m_rendererDllModuleName = "";
	std::string m_hitmanDlcDllModuleName = "";

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr double m_originalCameraFOV = 67.4;
	static constexpr double m_originalCutscenesFOV2 = 57.2957795130823;

	std::uint64_t m_cameraFOV1Bits = 0;
	std::uint64_t m_cameraFOV2Bits = 0;

	uint32_t m_cameraFOV1LowerBits = 0;
	uint32_t m_cameraFOV1HigherBits = 0;
	uint32_t m_cameraFOV2LowerBits = 0;
	uint32_t m_cameraFOV2HigherBits = 0;

	double m_newCutscenesFOV1 = 0.0;
	double m_newCutscenesFOV2 = 0.0;
	double m_newCameraFOV1 = 0.0;
	double m_newCameraFOV2 = 0.0;
	float m_newScopeViewFOV1 = 0.0f;
	float m_newScopeViewFOV2 = 0.0f;
	float m_newDrawDistance = 0.0f;
	float m_zoomFactor = 0.0f;
	float m_drawDistanceFactor = 0.0f;

	SafetyHookMid m_cutscenesFOV1Hook{};
	SafetyHookMid m_cutscenesFOV2Hook{};
	SafetyHookMid m_drawDistanceHook{};
	SafetyHookMid m_scopeViewFOV1Hook{};
	SafetyHookMid m_scopeViewFOV2Hook{};

	enum ResolutioninstructionsIndices
	{
		WidthHeight,
		ResList
	};

	enum CameraFOVInstructionsIndices
	{
		MouseConCam,
		BaseCam,
		MainCam,
		NewDialogCam,
		PlaneCam,
		StaticActivationCam,
		CutscenesCam1,
		CutscenesCam2,
		ScopeCam1,
		ScopeCam2
	};

	inline static HitmanCodename47Fix* s_instance_ = nullptr;
};

static std::unique_ptr<HitmanCodename47Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<HitmanCodename47Fix>(hModule);
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