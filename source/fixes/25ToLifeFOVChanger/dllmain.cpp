#include "..\..\common\FixBase.hpp"

class TwentyFiveToLifeFix final : public FixBase
{
public:
	explicit TwentyFiveToLifeFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~TwentyFiveToLifeFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "25ToLifeFOVChanger";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "25 To Life";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "TTL.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "HipfireFOVFactor", m_hipfireFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);

		spdlog_confparse(m_hipfireFOVFactor);
		spdlog_confparse(m_zoomFactor);
	}

	void ApplyFix() override
	{
		auto CameraFOVInstructionsScansResult = Memory::PatternScan(ExeModule(), "8B 48 ?? 6A ?? 51 8B CE 68",
		"D8 48 ?? 6A ?? 51 8B CE D9 1C ?? E8 ?? ?? ?? ?? 8B 4C 24 ?? 51 8B CE E8 ?? ?? ?? ?? 5E C2 ?? ?? CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC FF 25");
		if (Memory::AreAllSignaturesValid(CameraFOVInstructionsScansResult) == true)
		{
			spdlog::info("Hipfire Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[Hipfire] - (std::uint8_t*)ExeModule());

			spdlog::info("Zoom Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVInstructionsScansResult[Zoom] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVInstructionsScansResult, Hipfire, Zoom, 0, 3);

			HipfireFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Hipfire], [](SafetyHookContext& ctx)
			{
				float& fCurrentHipfireFOV = Memory::ReadMem(ctx.eax + 0x1C);

				s_instance_->m_newHipfireFOV = fCurrentHipfireFOV * s_instance_->m_hipfireFOVFactor;

				ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newHipfireFOV);
			});

			ZoomFOVHook = safetyhook::create_mid(CameraFOVInstructionsScansResult[Zoom], [](SafetyHookContext& ctx)
			{
				float& fCurrentZoomFOV = Memory::ReadMem(ctx.eax + 0x1C);

				s_instance_->m_newZoomFOV = fCurrentZoomFOV / s_instance_->m_zoomFactor;

				FPU::FMUL(s_instance_->m_newZoomFOV);
			});
		}
	}

private:
	enum CameraFOVInstructionsIndices
	{
		Hipfire,
		Zoom
	};

	SafetyHookMid HipfireFOVHook{};
	SafetyHookMid ZoomFOVHook{};

	float m_hipfireFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;
	float m_newHipfireFOV = 0.0f;
	float m_newZoomFOV = 0.0f;

	inline static TwentyFiveToLifeFix* s_instance_ = nullptr;
};

static std::unique_ptr<TwentyFiveToLifeFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<TwentyFiveToLifeFix>(hModule);

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