#include "..\..\common\FixBase.hpp"

class RGO2001Fix final : public FixBase
{
public:
	explicit RGO2001Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~RGO2001Fix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "RolandGarrosFrenchOpen2001WidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Roland Garros French Open 2001";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "RG2001.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "MenuResolutionWidth", m_newMenuResX);
		inipp::get_value(ini.sections["Settings"], "MenuResolutionHeight", m_newMenuResY);
		inipp::get_value(ini.sections["Settings"], "MatchResolutionWidth", m_newMatchResX);
		inipp::get_value(ini.sections["Settings"], "MatchResolutionHeight", m_newMatchResY);
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);

		FallbackToDesktopResolution(m_newMenuResX, m_newMenuResY);
		FallbackToDesktopResolution(m_newMatchResX, m_newMatchResY);

		spdlog_confparse(m_newMenuResX);
		spdlog_confparse(m_newMenuResY);
		spdlog_confparse(m_newMatchResX);
		spdlog_confparse(m_newMatchResY);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newMatchResX) / static_cast<float>(m_newMatchResY);

		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		auto MenuResolutionScansResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 0D",
		"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF D5 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF D7 8B 0D", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 15",
		"81 38 ?? ?? ?? ?? 0F 95 ?? 84 C0 74 ?? 8B 0D");
		if (Memory::AreAllSignaturesValid(MenuResolutionScansResult) == true)
		{
			spdlog::info("Menu Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), MenuResolutionScansResult[MenuRes1] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), MenuResolutionScansResult[MenuRes2] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Resolution Instructions 3 Scan: Address is {:s}+{:x}", ExeName().c_str(), MenuResolutionScansResult[MenuRes3] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Resolution Instructions 4 Scan: Address is {:s}+{:x}", ExeName().c_str(), MenuResolutionScansResult[MenuRes4] - (std::uint8_t*)ExeModule());
			spdlog::info("Menu Resolution Instructions 5 Scan: Address is {:s}+{:x}", ExeName().c_str(), MenuResolutionScansResult[MenuRes5] - (std::uint8_t*)ExeModule());

			// Main menu resolution is 800x600
			Memory::Write(MenuResolutionScansResult[MenuRes1] + 6, m_newMenuResX);
			Memory::Write(MenuResolutionScansResult[MenuRes1] + 1, m_newMenuResY);
			Memory::Write(MenuResolutionScansResult[MenuRes2] + 6, m_newMenuResX);
			Memory::Write(MenuResolutionScansResult[MenuRes2] + 1, m_newMenuResY);
			Memory::Write(MenuResolutionScansResult[MenuRes3] + 6, m_newMenuResX);
			Memory::Write(MenuResolutionScansResult[MenuRes3] + 1, m_newMenuResY);
			Memory::Write(MenuResolutionScansResult[MenuRes4] + 6, m_newMenuResX);
			Memory::Write(MenuResolutionScansResult[MenuRes4] + 1, m_newMenuResY);
			Memory::Write(MenuResolutionScansResult[MenuRes5] + 2, m_newMenuResX);
		}

		auto ResolutionListScanResult = Memory::PatternScan(ExeModule(), "85 ?? ?? ?? ?? 66 C7 41 ?? ?? ??");
		if (ResolutionListScanResult)
		{
			spdlog::info("Resolution List Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListScanResult - (std::uint8_t*)ExeModule());

			// 400x300
			Memory::Write(ResolutionListScanResult + 9, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 15, (uint16_t)m_newMatchResY);

			// 512x384
			Memory::Write(ResolutionListScanResult + 24, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 30, (uint16_t)m_newMatchResY);

			// 640x400
			Memory::Write(ResolutionListScanResult + 39, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 45, (uint16_t)m_newMatchResY);

			// 640x480
			Memory::Write(ResolutionListScanResult + 54, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 60, (uint16_t)m_newMatchResY);

			// 800x600
			Memory::Write(ResolutionListScanResult + 69, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 75, (uint16_t)m_newMatchResY);

			// 1024x768
			Memory::Write(ResolutionListScanResult + 84, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 90, (uint16_t)m_newMatchResY);

			// 1280x1024
			Memory::Write(ResolutionListScanResult + 99, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 105, (uint16_t)m_newMatchResY);

			// 1600x1200
			Memory::Write(ResolutionListScanResult + 114, (uint16_t)m_newMatchResX);
			Memory::Write(ResolutionListScanResult + 120, (uint16_t)m_newMatchResY);
		}
		else
		{
			spdlog::info("Cannot locate the resolution list memory address.");
			return;
		}

		dllModule2 = Memory::GetHandle("rcMain.dll");
		dllModule2Name = Memory::GetModuleName(dllModule2);

		// Located in rcMain.Runn::RCamera::think
		auto CameraFOVScanResult = Memory::PatternScan(dllModule2, "D8 0D ?? ?? ?? ?? D9 9E ?? ?? ?? ?? DB 05 ?? ?? ?? ?? D8 C9 D8 0D ?? ?? ?? ?? D9 9E ?? ?? ?? ?? 5E");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", dllModule2Name.c_str(), CameraFOVScanResult - (std::uint8_t*)dllModule2);
			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", dllModule2Name.c_str(), CameraFOVScanResult + 20 - (std::uint8_t*)dllModule2);

			m_cameraHFOVAddress = Memory::GetPointerFromAddress(CameraFOVScanResult + 2, Memory::PointerMode::Absolute);
			m_cameraVFOVAddress = Memory::GetPointerFromAddress(CameraFOVScanResult + 22, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScanResult, 6);
			Memory::WriteNOPs(CameraFOVScanResult + 20, 6);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOVAddress, s_instance_->m_aspectRatioScale);
			});			

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScanResult + 20, [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOVAddress);
			});
		}
		else
		{
			spdlog::info("Cannot locate the camera FOV instructions scan memory address.");
			return;
		}
	}

private:
	HMODULE dllModule2 = nullptr;
	std::string dllModule2Name = "";

	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	uintptr_t m_cameraHFOVAddress = 0;
	uintptr_t m_cameraVFOVAddress = 0;

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;	

	int m_newMatchResX = 0;
	int m_newMatchResY = 0;
	int m_newMenuResX = 0;
	int m_newMenuResY = 0;

	enum MenuResolutionInstructionsScan
	{
		MenuRes1,
		MenuRes2,
		MenuRes3,
		MenuRes4,
		MenuRes5
	};

	void CameraFOVMidHook(uintptr_t FOVAddress, float arScale = 1.0f)
	{
		float& fCurrentCameraHFOV = Memory::ReadMem(FOVAddress);

		m_newCameraFOV = fCurrentCameraHFOV / arScale / m_fovFactor;

		FPU::FMUL(m_newCameraFOV);
	}

	inline static RGO2001Fix* s_instance_ = nullptr;
};

static std::unique_ptr<RGO2001Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<RGO2001Fix>(hModule);
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