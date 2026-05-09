#include "..\..\common\FixBase.hpp"

class AirborneTroopsFix final : public FixBase
{
public:
	explicit AirborneTroopsFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AirborneTroopsFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AirborneTroopsCountdownToDDayFOVFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Airborne Troops: Countdown to D-Day";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "AirborneTroops.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "HipfireFOVFactor", m_hipfireFOVFactor);
		inipp::get_value(ini.sections["Settings"], "AimZoomFactor", m_aimFOVFactor);
		spdlog_confparse(m_hipfireFOVFactor);
		spdlog_confparse(m_aimFOVFactor);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 8B 94 24", "A3 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? A1",
		"8B 0A 89 0D ?? ?? ?? ?? 8B 6A", "8B 08 89 0D ?? ?? ?? ?? 8B 50 ?? 89 15 ?? ?? ?? ?? C7 05");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Windowed Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Windowed1] - (std::uint8_t*)ExeModule());
			spdlog::info("Windowed Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Windowed2] - (std::uint8_t*)ExeModule());
			spdlog::info("Fullscreen Resolution Instructions 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Fullscreen1] - (std::uint8_t*)ExeModule());
			spdlog::info("Fullscreen Resolution Instructions 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[Fullscreen2] - (std::uint8_t*)ExeModule());

			m_windowedRes1Hook = safetyhook::create_mid(ResolutionScansResult[Windowed1], [](SafetyHookContext& ctx)
			{
				const int iCurrentWidth = Memory::ReadRegister(ctx.eax);
				const int iCurrentHeight = Memory::ReadRegister(ctx.ecx);

				s_instance_->UpdateAspectAndReexecuteFunction(iCurrentWidth, iCurrentHeight);
			});

			m_windowedRes2Hook = safetyhook::create_mid(ResolutionScansResult[Windowed2], [](SafetyHookContext& ctx)
			{
				const int iCurrentWidth = Memory::ReadRegister(ctx.eax);
				const int iCurrentHeight = Memory::ReadRegister(ctx.ecx);

				s_instance_->UpdateAspectAndReexecuteFunction(iCurrentWidth, iCurrentHeight);
			});

			m_fullscreenRes1Hook = safetyhook::create_mid(ResolutionScansResult[Fullscreen1], [](SafetyHookContext& ctx)
			{
				const int iCurrentWidth = Memory::ReadMem(ctx.edx);
				const int iCurrentHeight = Memory::ReadMem(ctx.edx + 0x4);

				s_instance_->UpdateAspectAndReexecuteFunction(iCurrentWidth, iCurrentHeight);
			});

			m_fullscreenRes2Hook = safetyhook::create_mid(ResolutionScansResult[Fullscreen2], [](SafetyHookContext& ctx)
			{
				const int iCurrentWidth = Memory::ReadMem(ctx.eax);
				const int iCurrentHeight = Memory::ReadMem(ctx.eax + 0x4);

				s_instance_->UpdateAspectAndReexecuteFunction(iCurrentWidth, iCurrentHeight);
			});			
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "D9 05 ?? ?? ?? ?? D8 C9 D9 5C 24 ?? D8 0D ?? ?? ?? ?? D9 5C 24 ?? E8");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
			{
				s_instance_->g_AspectThis = ctx.esi;
				s_instance_->g_LastFOVParam = *reinterpret_cast<float*>(s_instance_->g_AspectThis + 0x160); // the game stores the "this" pointer in the [esi+0x160] object
				s_instance_->g_HasLastFOVParam = true;
			});
		}

		auto CameraFOVScansResult = Memory::PatternScan(ExeModule(), "A1 ?? ?? ?? ?? 89 96 ?? ?? ?? ?? 89 8E", "A1 ?? ?? ?? ?? D8 35");
		if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
		{
			spdlog::info("Hipfire FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Hipfire] - (std::uint8_t*)ExeModule());
			spdlog::info("Aím FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScansResult[Aim] - (std::uint8_t*)ExeModule());

			m_hipfireFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[Hipfire] + 1, Memory::PointerMode::Absolute);
			m_aimFOVAddress = Memory::GetPointerFromAddress(CameraFOVScansResult[Aim] + 1, Memory::PointerMode::Absolute);

			Memory::WriteNOPs(CameraFOVScansResult, Hipfire, Aim, 0, 5);

			m_hipfireFOVHook = safetyhook::create_mid(CameraFOVScansResult[Hipfire], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx, s_instance_->m_hipfireFOVAddress, 1.0f / s_instance_->m_hipfireFOVFactor);
			});

			m_aimFOVHook = safetyhook::create_mid(CameraFOVScansResult[Aim], [](SafetyHookContext& ctx)
			{
				s_instance_->CameraFOVMidHook(ctx, s_instance_->m_aimFOVAddress, s_instance_->m_aimFOVFactor);
			});
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	float m_aspectRatioScale = 1.0f;

	float m_hipfireFOVFactor = 0.0f;
	float m_aimFOVFactor = 0.0f;

	SafetyHookMid m_windowedRes1Hook{};
	SafetyHookMid m_windowedRes2Hook{};
	SafetyHookMid m_fullscreenRes1Hook{};
	SafetyHookMid m_fullscreenRes2Hook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_hipfireFOVHook{};
	SafetyHookMid m_aimFOVHook{};

	uintptr_t m_hipfireFOVAddress = 0;
	uintptr_t m_aimFOVAddress = 0;

	enum ResolutionInstructionsIndex
	{
		Windowed1,
		Windowed2,
		Fullscreen1,
		Fullscreen2
	};

	enum CameraFOVInstructionsIndex
	{
		Hipfire,
		Aim
	};

	void CameraFOVMidHook(SafetyHookContext& ctx, uintptr_t SourceAddress, float fovFactor)
	{
		float& fCurrentCameraFOV = Memory::ReadMem(SourceAddress);

		m_newCameraFOV = fCurrentCameraFOV * fovFactor;

		ctx.eax = std::bit_cast<uintptr_t>(m_newCameraFOV);
	}

	uintptr_t g_AspectThis = 0;
	float g_LastFOVParam = 0.0f;
	bool g_HasLastFOVParam = false;
	bool g_CallingAspectFunction = false;

	int g_LastWidth = 0;
	int g_LastHeight = 0;

	using AspectFunction = void(__thiscall*)(void* thisptr, float fov);

	AspectFunction CallAspectFunction = reinterpret_cast<AspectFunction>(reinterpret_cast<uintptr_t>(ExeModule()) + 0x72100);

	bool g_AspectFunctionHasRun = false;

	void UpdateAspectAndReexecuteFunction(int width, int height)
	{
		if (width == g_LastWidth && height == g_LastHeight)
		{
			return;
		}

		m_newAspectRatio = static_cast<float>(width) / static_cast<float>(height);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;

		m_newAspectRatio2 = 36.0f * m_aspectRatioScale;

		Memory::Write(reinterpret_cast<uintptr_t>(ExeModule()) + 0x216200, m_newAspectRatio2);

		if (g_AspectThis == false || g_HasLastFOVParam == false)
		{
			return;
		}

		if (g_CallingAspectFunction == true)
		{
			return;
		}

		g_CallingAspectFunction = true;

		CallAspectFunction(reinterpret_cast<void*>(g_AspectThis), g_LastFOVParam);

		g_CallingAspectFunction = false;
	}

	inline static AirborneTroopsFix* s_instance_ = nullptr;
};

static std::unique_ptr<AirborneTroopsFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AirborneTroopsFix>(hModule);
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