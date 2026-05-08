#include "..\..\common\FixBase.hpp"
#include <shlobj.h>
#pragma comment(lib, "Shell32.lib")

class Athens2004Fix final : public FixBase
{
	public:
		explicit Athens2004Fix(HMODULE selfModule) : FixBase(selfModule)
		{
			s_instance_ = this;
		}

		~Athens2004Fix() override
		{
			if (s_instance_ == this)
			{
				s_instance_ = nullptr;
			}
		}

	protected:
		const char* FixName() const override
		{
			return "Athens2004WidescreenFix";
		}

		const char* FixVersion() const override
		{
			return "1.1";
		}

		const char* TargetName() const override
		{
			return "Athens 2004";
		}

		InitMode GetInitMode() const override
		{
			return InitMode::Direct;
			// return InitMode::WorkerThread;
			// return InitMode::ExportedOnly;
		}

		bool IsCompatibleExecutable(const std::string& exeName) const override
		{
			return Util::stringcmp_caseless(exeName, "Athens.exe");
		}

		void ParseFixConfig(inipp::Ini<char>& ini) override
		{
			inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
			spdlog_confparse(m_fovFactor);
		}

		void ApplyFix() override
		{
			auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "56 8B 74 24 ?? 66 8B 06", "83 EC ?? 8B 0D ?? ?? ?? ?? 8B 01", "75 ?? 81 7D ?? ?? ?? ?? ?? 0F 84",
			"66 89 0D ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 6A");
			if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
			{
				spdlog::info("Resolution Limit Unlock 1 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResLimitUnlock1] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution Limit Unlock 2 Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResLimitUnlock2] - (std::uint8_t*)ExeModule());
				spdlog::info("Frontend Resolution Limit Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[FrontendResLimitUnlock] - (std::uint8_t*)ExeModule());
				spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ResWidthHeight] - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs((uint8_t*)ResolutionScansResult[ResLimitUnlock1], 95);

				Memory::PatchBytes((uint8_t*)ResolutionScansResult[ResLimitUnlock1], "\xC3");

				Memory::WriteNOPs((uint8_t*)ResolutionScansResult[ResLimitUnlock2], 3);

				Memory::PatchBytes((uint8_t*)ResolutionScansResult[ResLimitUnlock2], "\xC3");

				ReadAthensIniFrontendResolution();

				m_startupFrontendWidthBranchHook = safetyhook::create_mid(ResolutionScansResult[FrontendResLimitUnlock], [](SafetyHookContext& ctx)
				{
					uint32_t enumeratedWidth = Memory::ReadMem(ctx.ebp - 0x1C);

					bool widthMatchesIni = enumeratedWidth == static_cast<uint32_t>(s_instance_->m_frontendResX);

					SetZeroFlag(ctx, widthMatchesIni);
				});

				m_startupFrontendHeightBranchHook = safetyhook::create_mid(ResolutionScansResult[FrontendResLimitUnlock] + 9, [](SafetyHookContext& ctx)
				{
					uint32_t enumeratedHeight = Memory::ReadMem(ctx.ebp - 0x18);

					bool heightMatchesIni = enumeratedHeight == static_cast<uint32_t>(s_instance_->m_frontendResY);

					SetZeroFlag(ctx, heightMatchesIni);
				});

				m_mainMenuResolutionHook = safetyhook::create_mid(ResolutionScansResult[ResWidthHeight], [](SafetyHookContext& ctx)
				{
					uint16_t iCurrentMenuWidth = ctx.eax & 0xFFFF;
					uint16_t iCurrentMenuHeight = ctx.ecx & 0xFFFF;
					s_instance_->m_newAspectRatio = static_cast<float>(iCurrentMenuWidth) / static_cast<float>(iCurrentMenuHeight);
					s_instance_->m_aspectRatioScale = static_cast<float>(s_instance_->m_newAspectRatio) / static_cast<float>(s_instance_->m_oldAspectRatio);

					s_instance_->WriteGlobeFOV();
				});
			}

			CameraFOVScanResult = Memory::PatternScan(ExeModule(), "68 ?? ?? ?? ?? 53 8B CE");
			if (CameraFOVScanResult)
			{
				spdlog::info("Globe FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());
				spdlog::info("Gameplay FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), (uint8_t*)ExeModule() + 0x222C - (std::uint8_t*)ExeModule());

				Memory::WriteNOPs((uint8_t*)ExeModule() + 0x222C, 3);

				m_cameraFOVHook = safetyhook::create_mid((uint8_t*)ExeModule() + 0x222C, [](SafetyHookContext& ctx)
				{
					s_instance_->CameraFOVMidHook(ctx);
				});
			}
			else
			{
				spdlog::error("Failed to locate camera FOV instruction memory address.");
				return;
			}
		}

	private:
		static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
		float m_aspectRatioScale = 1.0f;

		SafetyHookMid m_startupFrontendWidthBranchHook{};
		SafetyHookMid m_startupFrontendHeightBranchHook{};
		SafetyHookMid m_mainMenuResolutionHook{};
		SafetyHookMid m_cameraFOVHook{};

		int m_frontendResX = 0;
		int m_frontendResY = 0;
		static constexpr uint32_t ZF_FLAG = 0x40;

		static constexpr float m_originalGlobeFOV = 0.5235987902f;
		float m_newGlobeFOV = 0.0f;
		float m_newGameplayFOV = 0.0f;

		uint8_t* CameraFOVScanResult;

		enum ResolutionInstructionsIndex
		{		
			ResLimitUnlock1,
			ResLimitUnlock2,
			FrontendResLimitUnlock,
			ResWidthHeight
		};

		std::filesystem::path GetAthensIniPath()
		{
			PWSTR documentsPath = nullptr;

			HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsPath);

			if (FAILED(hr) || documentsPath == nullptr)
			{
				spdlog::warn("SHGetKnownFolderPath failed. Falling back to USERPROFILE.");

				char userProfile[MAX_PATH] = {};
				DWORD result = GetEnvironmentVariableA("USERPROFILE", userProfile, MAX_PATH);

				if (result == 0 || result >= MAX_PATH)
				{
					return {};
				}

				return std::filesystem::path(userProfile) / "Documents" / "ATHENS 2004" / "Athens.ini";
			}

			std::filesystem::path athensIniPath = std::filesystem::path(documentsPath) / "ATHENS 2004" / "Athens.ini";

			CoTaskMemFree(documentsPath);

			return athensIniPath;
		}

		void ReadAthensIniFrontendResolution()
		{
			std::filesystem::path athensIniPath = GetAthensIniPath();

			if (athensIniPath.empty() == true)
			{
				spdlog::warn("Could not resolve Athens.ini path. Falling back to default resolution.");
				m_frontendResX = 640;
				m_frontendResY = 480;
				return;
			}

			if (std::filesystem::exists(athensIniPath) == false)
			{
				spdlog::warn("Athens.ini not found at {}. Falling back to default resolution.", athensIniPath.string());
				m_frontendResX = 640;
				m_frontendResY = 480;
				return;
			}

			inipp::Ini<char> athensIni;

			std::ifstream athensIniFile(athensIniPath);
			if (!athensIniFile)
			{
				spdlog::warn("Could not resolve Athens.ini path. Falling back to default resolution.");
				m_frontendResX = 640;
				m_frontendResY = 480;
				return;
			}

			athensIni.parse(athensIniFile);
			athensIni.strip_trailing_comments();

			bool gotWidth = inipp::get_value(athensIni.sections["Video"], "Frontend Width", m_frontendResX);
			bool gotHeight = inipp::get_value(athensIni.sections["Video"], "Frontend Height", m_frontendResY);

			if (!gotWidth || !gotHeight || m_frontendResX <= 0 || m_frontendResY <= 0)
			{
				spdlog::warn("Could not resolve Athens.ini path. Falling back to default resolution.");
				m_frontendResX = 640;
				m_frontendResY = 480;
			}
		}

		static void SetZeroFlag(SafetyHookContext& ctx, bool value)
		{
			if (value)
			{
				ctx.eflags |= ZF_FLAG;
			}
			else
			{
				ctx.eflags &= ~ZF_FLAG;
			}
		}

		void CameraFOVMidHook(SafetyHookContext& ctx)
		{
			float& fCurrentCameraFOV = Memory::ReadMem(ctx.edi + 0x70);

			m_newGameplayFOV = fCurrentCameraFOV * m_fovFactor;

			FPU::FLD(m_newGameplayFOV);
		}

		void WriteGlobeFOV()
		{
			m_newGlobeFOV = Maths::CalculateNewFOV_RadBased(m_originalGlobeFOV, m_aspectRatioScale, Maths::AngleMode::FullAngle);

			Memory::Write(CameraFOVScanResult + 1, m_newGlobeFOV);
		}

		inline static Athens2004Fix* s_instance_ = nullptr;
};

static std::unique_ptr<Athens2004Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);
		g_fix = std::make_unique<Athens2004Fix>(hModule);
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