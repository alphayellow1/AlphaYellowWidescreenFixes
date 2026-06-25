#include "..\..\common\FixBase.hpp"

class ShadowOfZorroFix final : public FixBase
{
public:
	explicit ShadowOfZorroFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~ShadowOfZorroFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TheShadowOfZorroWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1.1";
	}

	const char* TargetName() const override
	{
		return "The Shadow of Zorro";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "video.exe") ||
		Util::stringcmp_caseless(exeName, "Zorro.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{

	}

	void ApplyFix() override
	{
		if (Util::stringcmp_caseless(ExeName(), "video.exe"))
		{
			auto ResolutionListUnlockScansResult = Memory::PatternScan(ExeModule(), "7F ?? 3B 44 24", "7F ?? 47 3B C7");
			if (Memory::AreAllSignaturesValid(ResolutionListUnlockScansResult) == true)
			{
				spdlog::info("Resolution List Unlock Scan 1: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution List Unlock Scan 2: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListUnlockScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs(ResolutionListUnlockScansResult[ListUnlock1], 2);
				Memory::WriteNOPs(ResolutionListUnlockScansResult[ListUnlock2], 2);
			}
		}

		if (Util::stringcmp_caseless(ExeName(), "Zorro.exe"))
		{
			auto ResolutionScanResult = Memory::PatternScan(ExeModule(), "A1 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 2B C1");
			if (ResolutionScanResult)
			{
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScanResult - (std::uint8_t*)ExeModule());

				m_resolutionWidthAddress = Memory::GetPointerFromAddress(ResolutionScanResult + 28, Memory::PointerMode::Absolute);
				m_resolutionHeightAddress = Memory::GetPointerFromAddress(ResolutionScanResult + 1, Memory::PointerMode::Absolute);

				m_resolutionHook = safetyhook::create_mid(ResolutionScanResult, [](SafetyHookContext& ctx)
				{
					int& iCurrentWidth = Memory::ReadMem(s_instance_->m_resolutionWidthAddress);
					int& iCurrentHeight = Memory::ReadMem(s_instance_->m_resolutionHeightAddress);
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentWidth) / static_cast<float>(iCurrentHeight);
					s_instance_->m_aspectRatioScale = s_instance_->m_newAspectRatio / s_instance_->m_oldAspectRatio;
					s_instance_->m_resolutionHook.disable();
				});
			}
			else
			{
				spdlog::error("Failed to locate resolution instructions scan memory address.");
				return;
			}

			m_fnxCoreModule = Memory::GetHandle("fnx_core.dll");
			m_fnxCoreModuleName = Memory::GetModuleName(m_fnxCoreModule);
			m_fnxDX7Module = Memory::GetHandle("fnx_dx7.dll");
			m_fnxDX7ModuleName = Memory::GetModuleName(m_fnxDX7Module);

			auto AspectRatioScanResult = Memory::PatternScan(m_fnxCoreModule, "A1 ?? ?? ?? ?? 89 86 ?? ?? ?? ?? 8B C6");
			if (AspectRatioScanResult)
			{
				spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", m_fnxCoreModuleName.c_str(), AspectRatioScanResult - (std::uint8_t*)m_fnxCoreModule);

				Memory::WriteNOPs(AspectRatioScanResult, 5);

				m_aspectRatioHook = safetyhook::create_mid(AspectRatioScanResult, [](SafetyHookContext& ctx)
				{
					ctx.eax = std::bit_cast<uintptr_t>(s_instance_->m_newAspectRatio);
				});
			}
			else
			{
				spdlog::error("Failed to locate aspect ratio instruction memory address.");
				return;
			}

			auto CameraFOVScansResult = Memory::PatternScan(m_fnxDX7Module, "8B 8E ?? ?? ?? ?? 52", "D9 87 ?? ?? ?? ?? 8B CF");
			if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
			{
				spdlog::info("Camera FOV Instruction 1: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV1] - (std::uint8_t*)m_fnxDX7Module);
				spdlog::info("Camera FOV Instruction 2: Address is {:s}+{:x}", m_fnxDX7ModuleName.c_str(), CameraFOVScansResult[FOV2] - (std::uint8_t*)m_fnxDX7Module);

				Memory::WriteNOPs(CameraFOVScansResult[FOV1], 6);

				m_cameraFOV1Hook = safetyhook::create_mid(CameraFOVScansResult[FOV1], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV1 = Memory::ReadMem(ctx.esi + 0xCC);
					s_instance_->m_newCameraFOV1 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV1, s_instance_->m_aspectRatioScale);
					ctx.ecx = std::bit_cast<uintptr_t>(s_instance_->m_newCameraFOV1);
				});

				Memory::WriteNOPs(CameraFOVScansResult[FOV2], 6);

				m_cameraFOV2Hook = safetyhook::create_mid(CameraFOVScansResult[FOV2], [](SafetyHookContext& ctx)
				{
					float& fCurrentCameraFOV2 = Memory::ReadMem(ctx.edi + 0xCC);
					s_instance_->m_newCameraFOV2 = Maths::CalculateNewFOV_RadBased(fCurrentCameraFOV2, s_instance_->m_aspectRatioScale);
					FPU::FLD(s_instance_->m_newCameraFOV2);
				});
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	HMODULE m_fnxDX7Module = nullptr;
	std::string m_fnxDX7ModuleName = "";
	HMODULE m_fnxCoreModule = nullptr;
	std::string m_fnxCoreModuleName = "";

	int m_newDefaultWidth = 0;
	int m_newDefaultHeight = 0;

	float m_newCameraFOV1 = 0.0f;
	float m_newCameraFOV2 = 0.0f;

	uintptr_t m_resolutionWidthAddress = 0;
	uintptr_t m_resolutionHeightAddress = 0;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_aspectRatioHook{};
	SafetyHookMid m_cameraFOV1Hook{};
	SafetyHookMid m_cameraFOV2Hook{};

	enum ResolutionInstructionsIndex
	{
		ListUnlock1,
		ListUnlock2
	};

	enum CameraFOVInstructionsIndex
	{
		FOV1,
		FOV2
	};

	inline static ShadowOfZorroFix* s_instance_ = nullptr;
};

static std::unique_ptr<ShadowOfZorroFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<ShadowOfZorroFix>(hModule);
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