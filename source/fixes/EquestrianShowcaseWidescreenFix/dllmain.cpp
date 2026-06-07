#include "..\..\common\FixBase.hpp"

class EquestrianShowcaseFix final : public FixBase
{
public:
	explicit EquestrianShowcaseFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~EquestrianShowcaseFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "EquestrianShowcaseWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.1";
	}

	const char* TargetName() const override
	{
		return "Equestrian Showcase";
	}

	InitMode GetInitMode() const override
	{
		// return InitMode::Direct;
		return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "eq2001.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{

	}

	void ApplyFix() override
	{
		auto ResolutionListScansResult = Memory::PatternScan(ExeModule(), "0F 8C ?? ?? ?? ?? 81 7D", "75 ?? 81 7d ?? ?? ?? ?? ?? 7f");
		if (Memory::AreAllSignaturesValid(ResolutionListScansResult) == true)
		{
			spdlog::info("Resolution List Unlock 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListScansResult[ListUnlock1] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution List Unlock 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionListScansResult[ListUnlock2] - (std::uint8_t*)ExeModule());

			// 640x480 minimum check
			Memory::WriteNOPs(ResolutionListScansResult[ListUnlock1], 6);
			Memory::WriteNOPs(ResolutionListScansResult[ListUnlock1] + 13, 6);
			// 1024x768 maximum check
			Memory::PatchBytes(ResolutionListScansResult[ListUnlock2], "\xEB");
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_cameraFOVHook{};

	int m_newMenuWidth = 0;
	int m_newMenuHeight = 0;

	enum ResolutionInstructionsIndex
	{
		ListUnlock1,
		ListUnlock2
	};

	inline static EquestrianShowcaseFix* s_instance_ = nullptr;
};

static std::unique_ptr<EquestrianShowcaseFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<EquestrianShowcaseFix>(hModule);
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