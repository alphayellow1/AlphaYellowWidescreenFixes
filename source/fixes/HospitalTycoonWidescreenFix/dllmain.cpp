#include "..\..\common\FixBase.hpp"
#include <atomic>
#include <thread>
#include <chrono>

class HospitalTycoonFix final : public FixBase
{
public:
	explicit HospitalTycoonFix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~HospitalTycoonFix() override
	{
		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "HospitalTycoonWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.2.1";
	}

	const char* TargetName() const override
	{
		return "Hospital Tycoon";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "HospitalTycoon.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "FOVFactor", m_fovFactor);
		inipp::get_value(ini.sections["Settings"], "RunMultipleInstances", m_runMultipleInstances);
		spdlog_confparse(m_fovFactor);
		spdlog_confparse(m_runMultipleInstances);
	}

	void ApplyFix() override
	{
		auto ResolutionScansResult = Memory::PatternScan(ExeModule(), "0F 82 ?? ?? ?? ?? 8B 5A", "8B 54 24 ?? 8B 44 24 ?? 89 6E");
		if (Memory::AreAllSignaturesValid(ResolutionScansResult) == true)
		{
			spdlog::info("Resolution List Unlock Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[ListUnlock] - (std::uint8_t*)ExeModule());
			spdlog::info("Resolution Instructions Scan: Address is {:s}+{:x}", ExeName().c_str(), ResolutionScansResult[WidthHeight] - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(ResolutionScansResult[ListUnlock], 6);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock] + 15, 6);
			Memory::WriteNOPs(ResolutionScansResult[ListUnlock] + 26, 6);

			m_resolutionHook = safetyhook::create_mid(ResolutionScansResult[WidthHeight], [](SafetyHookContext& ctx)
			{
				int& iCurrentWidth = Memory::ReadMem(ctx.esp + 0x30);
				int& iCurrentHeight = Memory::ReadMem(ctx.esp + 0x34);
				s_instance_->m_newAspectRatio = static_cast<double>(iCurrentWidth) / static_cast<double>(iCurrentHeight);
			});
		}

		auto AspectRatioScanResult = Memory::PatternScan(ExeModule(), "DD 05 ?? ?? ?? ?? D8 C9 D9 5C 24 ?? D9 86");
		if (AspectRatioScanResult)
		{
			spdlog::info("Aspect Ratio Instruction: Address is {:s}+{:x}", ExeName().c_str(), AspectRatioScanResult - (std::uint8_t*)ExeModule());

			Memory::Write(AspectRatioScanResult + 2, &m_newAspectRatio);
		}
		else
		{
			spdlog::error("Failed to locate aspect ratio instruction memory address.");
			return;
		}

		auto CameraFOVScanResult = Memory::PatternScan(ExeModule(), "D9 81 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC CC 51 D9 81 ?? ?? ?? ?? D9 1C 24");
		if (CameraFOVScanResult)
		{
			spdlog::info("Camera FOV Instruction: Address is {:s}+{:x}", ExeName().c_str(), CameraFOVScanResult - (std::uint8_t*)ExeModule());

			Memory::WriteNOPs(CameraFOVScanResult, 6);

			m_cameraFOVHook = safetyhook::create_mid(CameraFOVScanResult, [](SafetyHookContext& ctx)
			{
				float& fCurrentCameraFOV = Memory::ReadMem(ctx.ecx + 0x1E8);
				s_instance_->m_newCameraFOV = fCurrentCameraFOV * s_instance_->m_fovFactor;
				FPU::FLD(s_instance_->m_newCameraFOV);
			});
		}
		else
		{
			spdlog::error("Failed to locate camera FOV instruction memory address.");
			return;
		}

		if (m_runMultipleInstances == true)
		{
			auto RunMultipleInstancesCheckScanResult = Memory::PatternScan(ExeModule(), "75 ?? A1 ?? ?? ?? ?? 56");
			if (RunMultipleInstancesCheckScanResult)
			{
				spdlog::info("Multiple Instance Check Instruction: Address is {:s}+{:x}", ExeName().c_str(), RunMultipleInstancesCheckScanResult - (std::uint8_t*)ExeModule());

				Memory::PatchBytes(RunMultipleInstancesCheckScanResult, "\xEB");
			}
			else
			{
				spdlog::error("Failed to locate multiple instance check instruction memory address.");
				return;
			}
		}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;

	SafetyHookMid m_resolutionHook{};
	SafetyHookMid m_cameraFOVHook{};

	bool m_runMultipleInstances = false;

	double m_newAspectRatio = 0.0;

	enum ResolutionInstructionsIndex
	{
		ListUnlock,
		WidthHeight
	};

	inline static HospitalTycoonFix* s_instance_ = nullptr;
};

static std::unique_ptr<HospitalTycoonFix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hModule);
			g_fix = std::make_unique<HospitalTycoonFix>(hModule);
			g_fix->Start();
			break;
		}

		case DLL_PROCESS_DETACH:
		{
			if (g_fix)
			{
				g_fix->Shutdown();
				g_fix.reset();
			}

			break;
		}

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		default:
			break;
	}

	return TRUE;
}