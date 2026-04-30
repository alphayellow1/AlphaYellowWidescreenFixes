#include "..\..\common\FixBase.hpp"

class AdventurePinballFix final : public FixBase
{
public:
	explicit AdventurePinballFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~AdventurePinballFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "AdventurePinballForgottenIslandResolutionFix";
	}

	const char* FixVersion() const override
	{
		return "1.2";
	}

	const char* TargetName() const override
	{
		return "Adventure Pinball: Forgotten Island";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "PB.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
	}

	void ApplyFix() override
	{
		m_dllModule2 = Memory::GetHandle("D3DDrv.dll", 50, 0);

		m_dllModule3 = Memory::GetHandle("WinDrv.dll", 50, 0);

		auto ResolutionScansResult = Memory::PatternScan(m_dllModule2, "3D ?? ?? ?? ?? 72 ?? 75", m_dllModule3, "89 51 ?? 8B 55 ?? 89 41 ?? 8D 04 D5 ?? ?? ?? ?? 89 41 ?? EB ?? 85 D2",
		"8B 78 ?? EB ?? 8B 78 ?? 89 7D ?? 39 55");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution Instructions Scan 1: Address is D3DDrv.dll+{:x}", ResolutionScansResult[Res1] - (std::uint8_t*)m_dllModule2);
			spdlog::info("Resolution Instructions Scan 2: Address is WinDrv.dll+{:x}", ResolutionScansResult[Res2] - (std::uint8_t*)m_dllModule3);
			spdlog::info("Resolution Instructions Scan 3: Address is WinDrv.dll+{:x}", ResolutionScansResult[Res3] - (std::uint8_t*)m_dllModule3);

			/*
			Memory::Write(ResolutionScansResult[Res1] + 1, m_newResX);
			Memory::Write(ResolutionScansResult[Res1] + 12, m_newResY);
			Memory::Write(ResolutionScansResult[Res1] + 19, m_newResX);
			Memory::Write(ResolutionScansResult[Res1] + 28, m_newResY);
			Memory::Write(ResolutionScansResult[Res1] + 35, m_newResX);
			Memory::Write(ResolutionScansResult[Res1] + 44, m_newResY);
			Memory::Write(ResolutionScansResult[Res1] + 51, m_newResX);
			Memory::Write(ResolutionScansResult[Res1] + 60, m_newResY);
			*/
			Memory::Write(ResolutionScansResult[Res1] + 67, m_newResX);
			Memory::Write(ResolutionScansResult[Res1] + 76, m_newResY);

			Memory::WriteNOPs(ResolutionScansResult[Res2], 3);
			Memory::WriteNOPs(ResolutionScansResult[Res2] + 6, 3);
			Memory::WriteNOPs(ResolutionScansResult[Res3], 3);
			Memory::WriteNOPs(ResolutionScansResult[Res3] + 20, 3);

			m_resolutionWidth2Hook = safetyhook::create_mid(ResolutionScansResult[Res2], [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x64) = s_instance_->m_newResX;
			});

			m_resolutionHeight2Hook = safetyhook::create_mid(ResolutionScansResult[Res2] + 6, [](SafetyHookContext& ctx)
			{
				*reinterpret_cast<int*>(ctx.ecx + 0x68) = s_instance_->m_newResY;
			});			

			m_resolutionWidth3Hook = safetyhook::create_mid(ResolutionScansResult[Res3], [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(s_instance_->m_newResX);
			});

			m_resolutionHeight3Hook = safetyhook::create_mid(ResolutionScansResult[Res3] + 20, [](SafetyHookContext& ctx)
			{
				ctx.edi = std::bit_cast<uintptr_t>(s_instance_->m_newResY);
			});
		}
	}

private:
	enum ResolutionInstructionsIndices
	{
		Res1,
		Res2,
		Res3,
	};

	HMODULE m_dllModule2;
	HMODULE m_dllModule3;

	SafetyHookMid m_resolutionWidth2Hook{};
	SafetyHookMid m_resolutionHeight2Hook{};
	SafetyHookMid m_resolutionWidth3Hook{};
	SafetyHookMid m_resolutionHeight3Hook{};

	inline static AdventurePinballFix* s_instance_ = nullptr;
};

static std::unique_ptr<AdventurePinballFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<AdventurePinballFix>(hModule);
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