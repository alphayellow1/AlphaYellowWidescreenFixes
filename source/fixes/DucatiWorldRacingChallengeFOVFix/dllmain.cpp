#include "..\..\common\FixBase.hpp"

class DucatiWRCFix final : public FixBase
{
public:
	explicit DucatiWRCFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~DucatiWRCFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "DucatiWorldRacingChallengeFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Ducati World: Racing Challenge";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "ducati.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		spdlog_confparse(m_fovFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "66 8B 88 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 66 8B 90 ?? ?? ?? ?? 89 15 ?? ?? ?? ?? C3");
		if (ResolutionScanResult)
		{
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

			m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
			{
				int16_t& iCurrentWidth = Memory::ReadMem(ctx.eax + 0x117C);
				int16_t& iCurrentHeight = Memory::ReadMem(ctx.eax + 0x117E);
				s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
				s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
			});
		}
		else
		{
			spdlog::info("Failed to locate resolution instructions scan memory address.");
			return;
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "66 A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 68", "66 A3 ?? ?? ?? ?? 03 C8");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Camera HFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[HFOV] - (std::uint8_t*)ExeModule());

			spdlog::info("Camera VFOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[VFOV] - (std::uint8_t*)ExeModule());

			m_cameraHFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[HFOV] + 2, Memory::PointerMode::Absolute);

			m_cameraVFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[VFOV] + 2, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult[HFOV], 6);

			m_cameraHFOVHook = safetyhook::create_mid(CameraFOVScansResult[HFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraHFOVAddress, ctx);
			});

			Memory::WriteNOPs(CameraFOVScansResult[VFOV], 6);

			m_cameraVFOVHook = safetyhook::create_mid(CameraFOVScansResult[VFOV], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(s_instance_->m_cameraVFOVAddress, ctx);
			});
		}
	}

private:
	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraHFOVHook{};
	SafetyHookMid m_cameraVFOVHook{};

	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	uintptr_t m_cameraHFOVAddress = 0;
	uintptr_t m_cameraVFOVAddress = 0;

	int16_t m_newCameraFOV;

	enum CameraFOVInstructionsIndices
	{
		HFOV,
		VFOV
	};

	void CameraFOVMidHook(uintptr_t DestAddress, SafetyHookContext& ctx)
	{
		const int& iCurrentCameraFOV = ctx.eax & 0xFFFF;

		m_newCameraFOV = (int16_t)((iCurrentCameraFOV / m_aspectRatioScale) / m_fovFactor);

		*reinterpret_cast<int16_t*>(DestAddress) = m_newCameraFOV;
	}

	inline static DucatiWRCFix* s_instance_ = nullptr;
};

static std::unique_ptr<DucatiWRCFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<DucatiWRCFix>(hModule);
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