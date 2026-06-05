#include "..\..\common\FixBase.hpp"
#include "..\..\common\DllNotificationWatcher.cpp"

class Torrente1Fix final : public FixBase
{
public:
	explicit Torrente1Fix(HMODULE selfModule) : FixBase(selfModule)
	{
		s_instance_ = this;
	}

	~Torrente1Fix() override
	{
		if (m_gameWatcher)
		{
			m_gameWatcher->Stop();
			m_gameWatcher.reset();
		}

		if (s_instance_ == this)
		{
			s_instance_ = nullptr;
		}
	}

protected:
	const char* FixName() const override
	{
		return "TorrenteWidescreenFix";
	}

	const char* FixVersion() const override
	{
		return "1.3.2";
	}

	const char* TargetName() const override
	{
		return "Torrente";
	}

	InitMode GetInitMode() const override
	{
		return InitMode::Direct;
		// return InitMode::WorkerThread;
		// return InitMode::ExportedOnly;
	}

	bool IsCompatibleExecutable(const std::string& exeName) const override
	{
		return Util::stringcmp_caseless(exeName, "torrente.exe");
	}

	void ParseFixConfig(inipp::Ini<char>& ini) override
	{
		inipp::get_value(ini.sections["Settings"], "Width", m_newResX);
		inipp::get_value(ini.sections["Settings"], "Height", m_newResY);
		inipp::get_value(ini.sections["Settings"], "ThirdPersonFOVFactor", m_firstPersonFOVFactor);
		inipp::get_value(ini.sections["Settings"], "FirstPersonFOVFactor", m_thirdPersonFOVFactor);
		inipp::get_value(ini.sections["Settings"], "ZoomFactor", m_zoomFactor);

		FallbackToDesktopResolution(m_newResX, m_newResY);

		spdlog_confparse(m_newResX);
		spdlog_confparse(m_newResY);
		spdlog_confparse(m_firstPersonFOVFactor);
		spdlog_confparse(m_thirdPersonFOVFactor);
		spdlog_confparse(m_zoomFactor);
	}

	void ApplyFix() override
	{
		m_newAspectRatio = static_cast<float>(m_newResX) / static_cast<float>(m_newResY);
		m_aspectRatioScale = m_newAspectRatio / m_oldAspectRatio;
		m_newHUDWidth = (int)(800.0f * m_aspectRatioScale);

		m_vtKernelDllModule = Memory::GetHandle("vtKernel.dll");
		m_vtKernelDllModuleName = Memory::GetModuleName(m_vtKernelDllModule);

		// Located in vtEngine::vtEngine and vtEngine::SetResolution
		auto ResolutionListsScansResult = Memory::PatternScan(m_vtKernelDllModule, "C7 83 ?? ?? ?? ?? ?? ?? ?? ?? C7 83 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? 83 F8",
		"C7 86 ?? ?? ?? ?? ?? ?? ?? ?? C7 86 ?? ?? ?? ?? ?? ?? ?? ?? EB ?? 83 FB");
		if (Memory::AreAllSignaturesValid(ResolutionListsScansResult) == true)
		{
			spdlog::info("Resolution List 1 Scan: Address is {:s}+{:x}", m_vtKernelDllModuleName.c_str(), ResolutionListsScansResult[List1] - (std::uint8_t*)m_vtKernelDllModule);
			spdlog::info("Resolution List 2 Scan: Address is {:s}+{:x}", m_vtKernelDllModuleName.c_str(), ResolutionListsScansResult[List2] - (std::uint8_t*)m_vtKernelDllModule);

			// Resolution List 1
			// 640x480
			Memory::Write(ResolutionListsScansResult[List1] + 6, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 16, m_newResY);
			// 800x600
			Memory::Write(ResolutionListsScansResult[List1] + 33, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 43, m_newResY);
			// 1024x768
			Memory::Write(ResolutionListsScansResult[List1] + 55, m_newResX);
			Memory::Write(ResolutionListsScansResult[List1] + 65, m_newResY);

			// Resolution List 2
			// 640x480
			Memory::Write(ResolutionListsScansResult[List2] + 6, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 16, m_newResY);
			// 800x600
			Memory::Write(ResolutionListsScansResult[List2] + 33, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 43, m_newResY);
			// 1024x768
			Memory::Write(ResolutionListsScansResult[List2] + 55, m_newResX);
			Memory::Write(ResolutionListsScansResult[List2] + 65, m_newResY);
		}
		
		m_gameWatcher = std::make_unique<DllNotificationWatcher>(
			// OnLoad
			[this](HMODULE module)
			{
				const std::string moduleName = Memory::GetModuleName(module);

				if (IsWatchedModule(moduleName))
				{
					spdlog::info("{:s} loaded.", moduleName.c_str());
				}

				if (Util::stringcmp_caseless(moduleName, "camera.dll")) // Camera
				{
					auto CameraFOVScansResult = Memory::PatternScan(module, "b8 ?? ?? ?? ?? 3b c8 74 ?? 50 89 85 ?? ?? ?? ?? 8b 45 ?? 50 ff 15 ?? ?? ?? ?? 83 c4 ?? 8a 85 ?? ?? ?? ?? 84 c0 0f 84 ?? ?? ?? ?? 8b 8d",
					"68 ?? ?? ?? ?? 51 C7 86", "b8 ?? ?? ?? ?? 3b c8 74 ?? 50 89 85 ?? ?? ?? ?? 8b 45 ?? 50 ff 15 ?? ?? ?? ?? 83 c4 ?? 8a 85 ?? ?? ?? ?? 84 c0 0f 84 ?? ?? ?? ?? b9",
					"d8 25 ?? ?? ?? ?? d9 54 24", "d8 05 ?? ?? ?? ?? d9 54 24");
					if (Memory::AreAllSignaturesValid(CameraFOVScansResult) == true)
					{
						spdlog::info("Third-Person FOV Instruction: Address is {:s}+{:x}", moduleName.c_str(), CameraFOVScansResult[ThirdPerson] - (std::uint8_t*)module);
						spdlog::info("First-Person FOV Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), CameraFOVScansResult[FirstPerson1] - (std::uint8_t*)module);
						spdlog::info("First-Person FOV Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), CameraFOVScansResult[FirstPerson2] - (std::uint8_t*)module);
						spdlog::info("Sniper Zoom In FOV Instruction: Address is {:s}+{:x}", moduleName.c_str(), CameraFOVScansResult[SniperZoomIn] - (std::uint8_t*)module);
						spdlog::info("Sniper Zoom Out FOV Instruction: Address is {:s}+{:x}", moduleName.c_str(), CameraFOVScansResult[SniperZoomOut] - (std::uint8_t*)module);

						m_newThirdPersonFOV = Maths::CalculateNewFOV_RadBased(m_defaultCameraFOV, m_aspectRatioScale) * m_firstPersonFOVFactor;
						m_newFirstPersonFOV = Maths::CalculateNewFOV_RadBased(m_defaultCameraFOV, m_aspectRatioScale) * m_thirdPersonFOVFactor;

						Memory::Write(CameraFOVScansResult[ThirdPerson] + 1, m_newThirdPersonFOV);
						Memory::Write(CameraFOVScansResult[FirstPerson1] + 1, m_newFirstPersonFOV);
						Memory::Write(CameraFOVScansResult[FirstPerson2] + 1, m_newFirstPersonFOV);

						m_newSniperZoomInFOV = Maths::CalculateNewFOV_RadBased(0.2f, m_aspectRatioScale) * m_zoomFactor;
						m_newSniperZoomOutFOV = Maths::CalculateNewFOV_RadBased(0.2f, m_aspectRatioScale) / m_zoomFactor;

						Memory::Write(CameraFOVScansResult[SniperZoomIn] + 2, &m_newSniperZoomInFOV);
						Memory::Write(CameraFOVScansResult[SniperZoomOut] + 2, &m_newSniperZoomOutFOV);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "cochetorrente.dll")) // Bonus Level
				{
					auto CocheTorrenteHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 54 24", "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 68 ?? ?? ?? ?? 50",
					"68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 68 ?? ?? ?? ?? 50", "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 6A",
					"68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 68", "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 68",
					"68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 68 ?? ?? ?? ?? 55", "68 ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 50 8D 8C 24 ?? ?? ?? ?? 51 6A",
					"68 ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 50 89 0D", "BD ?? ?? ?? ?? 8D B8", "68 ?? ?? ?? ?? 50 FF 15");
					if (Memory::AreAllSignaturesValid(CocheTorrenteHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD7] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 8: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD8] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 9: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD9] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 10: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD10] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 11: Address is {:s}+{:x}", moduleName.c_str(), CocheTorrenteHUDScansResult[CocheTorrenteHUD11] - (std::uint8_t*)module);

						Memory::Write(CocheTorrenteHUDScansResult, CocheTorrenteHUD1, CocheTorrenteHUD9, 1, m_newHUDWidth);

						m_newCocheTorrenteHUDValue10 = (int)(std::round(598.5f * m_newAspectRatio - 248.0f));
						m_newCocheTorrenteHUDFOV = Maths::CalculateNewFOV_RadBased(1.27999997138977f, m_aspectRatioScale);
						Memory::Write(CocheTorrenteHUDScansResult[CocheTorrenteHUD10] + 2, m_newCocheTorrenteHUDValue10);
						Memory::Write(CocheTorrenteHUDScansResult[CocheTorrenteHUD11] + 1, m_newCocheTorrenteHUDFOV);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "comercial.dll")) // Shopping Center Timer Counter
				{
					auto ComercialHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 B9",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51", "68 ?? ?? ?? ?? 8D 44 24", "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 83 C4",
					"68 ?? ?? ?? ?? 8D 84 24", "68 ?? ?? ?? ?? FF D6 83 C4", "68 ?? ?? ?? ?? FF D6 B9");
					if (Memory::AreAllSignaturesValid(ComercialHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), ComercialHUDScansResult[ComercialHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), ComercialHUDScansResult[ComercialHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), ComercialHUDScansResult[ComercialHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), ComercialHUDScansResult[ComercialHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), ComercialHUDScansResult[ComercialHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), ComercialHUDScansResult[ComercialHUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), ComercialHUDScansResult[ComercialHUD7] - (std::uint8_t*)module);

						Memory::Write(ComercialHUDScansResult, ComercialHUD1, ComercialHUD5, 1, m_newHUDWidth);

						m_newComercialHUDValue6 = (int)(std::round(600.0f * m_newAspectRatio - 30.0f));
						m_newComercialHUDValue7 = (int)(std::round(600.0f * m_newAspectRatio - 62.0f));
						Memory::Write(ComercialHUDScansResult[ComercialHUD6] + 1, m_newComercialHUDValue6);						
						Memory::Write(ComercialHUDScansResult[ComercialHUD7] + 1, m_newComercialHUDValue7);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "contadordetonador.dll")) // Detonation Counter
				{
					auto ContadorDetonadorHUDScansResult = Memory::PatternScan(module, "68 58 02 00 00 68 20 03 00 00 8D 44 24 44 50 8D 4C 24 44 51 68 2F 02 00 00 68 E3",
					"68 58 02 00 00 68 20 03 00 00 8D 4C 24 44 51 8D 54 24 44 52 68 3E 02 00 00 68 00 03 00 00", "68 58 02 00 00 68 20 03 00 00 8D 44 24 54 50 8D 4C 24 54 51 6A 27 6A 34 FF D5 8B 54 24 64 8B",
					"68 58 02 00 00 68 20 03 00 00 8D 54 24 78 52 8D 44 24 78 50 6A 11 6A 11 FF D5 8B 8C 24 88", "68 E3 02 00 00 89 5C 24 44 C7 06 80 20 00 10 FF 15 ?? ?? ?? ??",
					"68 00 03 00 00 FF 15 ?? ?? ?? ?? B8 F0 30 00 10 83 C4 18 85 C0");
					if (Memory::AreAllSignaturesValid(ContadorDetonadorHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), ContadorDetonadorHUDScansResult[ContadorDetonadorHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), ContadorDetonadorHUDScansResult[ContadorDetonadorHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), ContadorDetonadorHUDScansResult[ContadorDetonadorHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), ContadorDetonadorHUDScansResult[ContadorDetonadorHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), ContadorDetonadorHUDScansResult[ContadorDetonadorHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), ContadorDetonadorHUDScansResult[ContadorDetonadorHUD6] - (std::uint8_t*)module);

						Memory::Write(ContadorDetonadorHUDScansResult, ContadorDetonadorHUD1, ContadorDetonadorHUD4, 6, m_newHUDWidth);

						m_newContadorDetonadorHUDValue5 = (int)(std::round(600.0f * m_newAspectRatio - 61.0f));
						m_newContadorDetonadorHUDValue6 = (int)(std::round(600.0f * m_newAspectRatio - 32.0f));
						Memory::Write(ContadorDetonadorHUDScansResult[ContadorDetonadorHUD2] + 1, m_newContadorDetonadorHUDValue5);
						Memory::Write(ContadorDetonadorHUDScansResult[ContadorDetonadorHUD4] + 1, m_newContadorDetonadorHUDValue6);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "contadordinero.dll")) // Money Counter
				{
					auto ContadorDineroHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 44 24", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 68",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A", "68 ?? ?? ?? ?? 89 5C 24", "68 ?? ?? ?? ?? FF D5 B9");
					if (Memory::AreAllSignaturesValid(ContadorDineroHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), ContadorDineroHUDScansResult[ContadorDineroHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), ContadorDineroHUDScansResult[ContadorDineroHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), ContadorDineroHUDScansResult[ContadorDineroHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), ContadorDineroHUDScansResult[ContadorDineroHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), ContadorDineroHUDScansResult[ContadorDineroHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), ContadorDineroHUDScansResult[ContadorDineroHUD6] - (std::uint8_t*)module);

						Memory::Write(ContadorDineroHUDScansResult, ContadorDineroHUD1, ContadorDineroHUD4, 1, m_newHUDWidth);

						m_newContadorDineroHUDValue5 = (int)(std::round(600.0f * m_newAspectRatio - 166.0f));
						m_newContadorDineroHUDValue6 = (int)(std::round(600.0f * m_newAspectRatio - 130.0f));
						Memory::Write(ContadorDineroHUDScansResult[ContadorDineroHUD5] + 1, m_newContadorDineroHUDValue5);
						Memory::Write(ContadorDineroHUDScansResult[ContadorDineroHUD6] + 1, m_newContadorDineroHUDValue6);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "contadormunicion.dll")) // Ammo Counter
				{
					auto ContadorMunicionHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 44 24", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 68",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A");
					if (Memory::AreAllSignaturesValid(ContadorMunicionHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), ContadorMunicionHUDScansResult[ContadorMunicionHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), ContadorMunicionHUDScansResult[ContadorMunicionHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), ContadorMunicionHUDScansResult[ContadorMunicionHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), ContadorMunicionHUDScansResult[ContadorMunicionHUD4] - (std::uint8_t*)module);

						Memory::Write(ContadorMunicionHUDScansResult, ContadorMunicionHUD1, ContadorMunicionHUD4, 1, m_newHUDWidth);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "contadorsalud.dll")) // Health Counter
				{
					auto ContadorSaludHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8d 44 24 ?? 50 8d 4c 24 ?? 51 68", "68 ?? ?? ?? ?? 8d 4c 24 ?? 51 8d 54 24 ?? 52 68",
					"68 ?? ?? ?? ?? 8d 44 24 ?? 50 8d 4c 24 ?? 51 6a", "68 ?? ?? ?? ?? 8d 54 24 ?? 52 8d 44 24 ?? 50 6a", "68 ?? ?? ?? ?? 8d 54 24 ?? 52 8d 44 24 ?? 50 68",
					"68 ?? ?? ?? ?? 8d 4c 24 ?? 51 8d 54 24 ?? 52 6a", "68 ?? ?? ?? ?? 8d 4c 24 ?? 51 8d 54 24 ?? 2b c6", "68 ?? ?? ?? ?? 8d 4c 24 ?? 51 8d 54 24 ?? 52 56");
					if (Memory::AreAllSignaturesValid(ContadorSaludHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD7] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 8: Address is {:s}+{:x}", moduleName.c_str(), ContadorSaludHUDScansResult[ContadorSaludHUD8] - (std::uint8_t*)module);

						Memory::Write(ContadorSaludHUDScansResult, ContadorSaludHUD1, ContadorSaludHUD8, 1, m_newHUDWidth);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "contadortiempo.dll")) // Time Counter
				{
					auto ContadorTiempoHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 68 ?? ?? ?? ?? FF D5 B9",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51", "68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 6A ?? 68", "68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 6A ?? 6A",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 68 ?? ?? ?? ?? FF D5 8B 8C 24", "68 ?? ?? ?? ?? FF D5 B9", "68 ?? ?? ?? ?? FF D5 68");
					if (Memory::AreAllSignaturesValid(ContadorTiempoHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), ContadorTiempoHUDScansResult[ContadorTiempoHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), ContadorTiempoHUDScansResult[ContadorTiempoHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), ContadorTiempoHUDScansResult[ContadorTiempoHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), ContadorTiempoHUDScansResult[ContadorTiempoHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), ContadorTiempoHUDScansResult[ContadorTiempoHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), ContadorTiempoHUDScansResult[ContadorTiempoHUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), ContadorTiempoHUDScansResult[ContadorTiempoHUD7] - (std::uint8_t*)module);

						Memory::Write(ContadorTiempoHUDScansResult, ContadorTiempoHUD1, ContadorTiempoHUD5, 1, m_newHUDWidth);

						m_newContadorTiempoHUDValue6 = (int)(std::round(600.0f * m_newAspectRatio - 175.0f));
						m_newContadorTiempoHUDValue7 = (int)(std::round(600.0f * m_newAspectRatio - 178.0f));
						Memory::Write(ContadorTiempoHUDScansResult[ContadorTiempoHUD6] + 1, m_newContadorTiempoHUDValue6);
						Memory::Write(ContadorTiempoHUDScansResult[ContadorTiempoHUD7] + 1, m_newContadorTiempoHUDValue7);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "fase_cuco.dll")) // Sniper
				{
					auto FaseCucoHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF 15",
					"68 ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 50 8D 8C 24 ?? ?? ?? ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? C6 44 24",
					"68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? C6 44 24",
					"68 20 03 00 00 8D 84 24 88 00 00 00 50 8D 8C 24 88 00 00 00 51 68 30 02 00 00 68 A1",
					"68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68",
					"68 20 03 00 00 8D 84 24 88 00 00 00 50 8D 8C 24 88 00 00 00 51 68 30 02 00 00 68 AD",
					"68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53",
					"68 ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 50 8D 8C 24 ?? ?? ?? ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B F8",
					"68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B F8",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51",
					"68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? C6 44 24",
					"68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68",
					"68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 53",
					"68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B F8");
					if (Memory::AreAllSignaturesValid(FaseCucoHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD7] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 8: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD8] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 9: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD9] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 10: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD10] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 11: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD11] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 12: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD12] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 13: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD13] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 14: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD14] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 15: Address is {:s}+{:x}", moduleName.c_str(), FaseCucoHUDScansResult[FaseCucoHUD15] - (std::uint8_t*)module);

						Memory::Write(FaseCucoHUDScansResult, FaseCucoHUD1, FaseCucoHUD11, 1, m_newHUDWidth);

						m_newFaseCucoHUDValue12 = (int)(std::round(600.0f * m_newAspectRatio - 97.0f));
						m_newFaseCucoHUDValue13 = (int)(std::round(600.0f * m_newAspectRatio - 127.0f));
						m_newFaseCucoHUDValue14 = (int)(std::round(600.0f * m_newAspectRatio - 115.0f));
						m_newFaseCucoHUDValue15 = (int)(std::round(600.0f * m_newAspectRatio - 60.0f));
						Memory::Write(FaseCucoHUDScansResult[FaseCucoHUD12] + 1, m_newFaseCucoHUDValue12);
						Memory::Write(FaseCucoHUDScansResult[FaseCucoHUD13] + 1, m_newFaseCucoHUDValue13);
						Memory::Write(FaseCucoHUDScansResult[FaseCucoHUD14] + 1, m_newFaseCucoHUDValue14);
						Memory::Write(FaseCucoHUDScansResult[FaseCucoHUD15] + 1, m_newFaseCucoHUDValue15);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "fase_spinelli.dll")) // Final Level
				{
					auto FaseSpinelliHUDScanResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51");
					if (FaseSpinelliHUDScanResult)
					{
						spdlog::info("HUD Instruction: Address is {:s}+{:x}", moduleName.c_str(), FaseSpinelliHUDScanResult - (std::uint8_t*)module);

						Memory::Write(FaseSpinelliHUDScanResult + 1, m_newHUDWidth);
					}
					else
					{
						spdlog::info("Cannot locate the HUD instruction memory address (FASE_SPINELLI.dll).");
						return;
					}
				}

				if (Util::stringcmp_caseless(moduleName, "madrid_f03.dll")) // Madrid 03
				{
					auto Madrid_F03HUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 6A",
					"68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 8C 24 ?? ?? ?? ?? 51 6A", "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 68",
					"68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 68", "68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 8C 24 ?? ?? ?? ?? 51 42",
					"68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 6A",
					"68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 8C 24 ?? ?? ?? ?? 8B 94 24 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68");
					if (Memory::AreAllSignaturesValid(Madrid_F03HUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F03HUDScansResult[Madrid_F03HUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F03HUDScansResult[Madrid_F03HUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F03HUDScansResult[Madrid_F03HUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F03HUDScansResult[Madrid_F03HUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F03HUDScansResult[Madrid_F03HUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F03HUDScansResult[Madrid_F03HUD6] - (std::uint8_t*)module);

						Memory::Write(Madrid_F03HUDScansResult, Madrid_F03HUD1, Madrid_F03HUD4, 1, m_newHUDWidth);

						m_newMadrid_F03HUDValue5 = (int)(std::round(210.0f * m_newAspectRatio - 31.0f));						
						m_newMadrid_F03HUDValue6 = (int)(std::round(210.0f * m_newAspectRatio - 35.0f));
						Memory::Write(Madrid_F03HUDScansResult[Madrid_F03HUD3] + 1, m_newMadrid_F03HUDValue5);
						Memory::Write(Madrid_F03HUDScansResult[Madrid_F03HUD5] + 1, m_newMadrid_F03HUDValue6);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "madrid_f04.dll")) // Madrid 04
				{
					auto Madrid_F04HUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? C7 44 24 ?? ?? ?? ?? ?? 8B 54 24 ?? 89 51", "68 ?? ?? ?? ?? 8D 44 24",
					"68 ?? ?? ?? ?? 8D 94 24", "68 ?? ?? ?? ?? FF D7");
					if (Memory::AreAllSignaturesValid(Madrid_F04HUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F04HUDScansResult[Madrid_F04HUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F04HUDScansResult[Madrid_F04HUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F04HUDScansResult[Madrid_F04HUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), Madrid_F04HUDScansResult[Madrid_F04HUD4] - (std::uint8_t*)module);

						Memory::Write(Madrid_F04HUDScansResult, Madrid_F04HUD1, Madrid_F04HUD3, 1, m_newHUDWidth);

						m_newMadrid_F04HUDValue4 = (int)(std::round(600.0f * m_newAspectRatio - 44.0f));
						Memory::Write(Madrid_F04HUDScansResult[Madrid_F04HUD4] + 1, m_newMadrid_F04HUDValue4);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "madrid_franco.dll")) // Madrid Sniper Level
				{
					auto Madrid_FrancoScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8B 8E",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 B9",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D6 8B 44 24 ?? 8B 4C 24 ?? 8B 95 ?? ?? ?? ?? 50 51 52 FF 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 BA",
					"68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 BA",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D6 8B 44 24 ?? 8B 4C 24 ?? 8B 95 ?? ?? ?? ?? 50 51 52 FF 15 ?? ?? ?? ?? 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 68",
					"68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24", "68 ?? ?? ?? ?? 8D 94 24",
					"68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF 15", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D7 8B 44 24",
					"68 ?? ?? ?? ?? 8D 84 24", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF 15", "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 68",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D6 83 C4", "68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D6 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 6A ?? FF D6",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 4C 24",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B 4C 24",
					"68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D7", "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 6A ?? FF D7",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 68", "68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 6A",
					"68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B F8",
					"68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D7 83 C4 ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? FF 15 ?? ?? ?? ?? 8B F8", "68 ?? ?? ?? ?? FF D6 B9",
					"68 ?? ?? ?? ?? FF D6 BA", "68 ?? ?? ?? ?? FF D6 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24");
					if (Memory::AreAllSignaturesValid(Madrid_FrancoScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD7] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 8: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD8] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 9: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD9] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 10: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD10] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 11: Address is {:s}+{:x}", moduleName.c_str(), Madrid_FrancoScansResult[Madrid_FrancoHUD11] - (std::uint8_t*)module);

						Memory::Write(Madrid_FrancoScansResult, Madrid_FrancoHUD1, Madrid_FrancoHUD24, 1, m_newHUDWidth);

						m_newMadrid_FrancoHUDValue25 = (int)(std::round(600.0f * m_newAspectRatio - 161.0f));
						m_newMadrid_FrancoHUDValue26 = (int)(std::round(600.0f * m_newAspectRatio - 113.0f));
						m_newMadrid_FrancoHUDValue27 = (int)(std::round(412.5f * m_newAspectRatio + 139.0f));

						Memory::Write(Madrid_FrancoScansResult[Madrid_FrancoHUD25] + 1, m_newMadrid_FrancoHUDValue25);						
						Memory::Write(Madrid_FrancoScansResult[Madrid_FrancoHUD26] + 1, m_newMadrid_FrancoHUDValue26);						
						Memory::Write(Madrid_FrancoScansResult[Madrid_FrancoHUD27] + 1, m_newMadrid_FrancoHUDValue27);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "madrid_kio.dll"))
				{
					auto Madrid_KioHUDScanResult = Memory::PatternScan(module, "68 58 02 00 00 68 20 03 00 00 8D 8C 24 94 00 00 00 51 8D 94 24");
					if (Madrid_KioHUDScanResult)
					{
						spdlog::info("HUD Instruction: Address is {:s}+{:x}", moduleName.c_str(), Madrid_KioHUDScanResult - (std::uint8_t*)module);

						Memory::Write(Madrid_KioHUDScanResult + 6, m_newHUDWidth);
					}
					else
					{
						spdlog::info("Cannot locate the HUD instruction memory address (madrid_kio.dll).");
						return;
					}
				}

				if (Util::stringcmp_caseless(moduleName, "main.dll")) // Main Menu
				{
					auto MainHUDScansResult = Memory::PatternScan(module, "06 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00",
					"06 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00 8D 44 24 30 50 8D 4C 24 38 51 68 58 02 00 00 68 20 03 00 00",
					"00 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00",
					"00 88 9E 08 01 00 00 0F 85 27 01 00 00 68 58 02 00 00 68 20 03 00 00 8D 44 24 30 50 8D 4C 24 38 51 68 58 02 00 00 68 20 03 00 00");
					if (Memory::AreAllSignaturesValid(MainHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), MainHUDScansResult[MainHUD1] + 18 - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), MainHUDScansResult[MainHUD2] + 38 - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), MainHUDScansResult[MainHUD3] + 18 - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), MainHUDScansResult[MainHUD4] + 38 - (std::uint8_t*)module);

						Memory::Write(MainHUDScansResult[MainHUD1] + 19, m_newHUDWidth);
						Memory::Write(MainHUDScansResult[MainHUD2] + 39, m_newHUDWidth);
						Memory::Write(MainHUDScansResult[MainHUD3] + 19, m_newHUDWidth);
						Memory::Write(MainHUDScansResult[MainHUD4] + 39, m_newHUDWidth);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "marbella_chalets.dll")) // Marbella Mansions Level
				{
					auto Marbella_ChaletsHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A",
					"68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 68", "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 6A ?? FF D7",
					"68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 68", "68 ?? ?? ?? ?? 8D 8C 24 ?? ?? ?? ?? 51 8D 94 24 ?? ?? ?? ?? 52 6A ?? 6A ?? FF 15",
					"68 ?? ?? ?? ?? FF 15 ?? ?? ?? ?? B9", "68 ?? ?? ?? ?? FF D7");
					if (Memory::AreAllSignaturesValid(Marbella_ChaletsHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD7] - (std::uint8_t*)module);

						Memory::Write(Marbella_ChaletsHUDScansResult, Marbella_ChalettsHUD1, Marbella_ChalettsHUD5, 1, m_newHUDWidth);

						m_newMarbella_ChaletsHUDValue6 = (int)(std::round(600.0f * m_newAspectRatio - 53.0f));
						m_newMarbella_ChaletsHUDValue7 = (int)(std::round(600.0f * m_newAspectRatio - 30.0f));
						Memory::Write(Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD6] + 1, m_newMarbella_ChaletsHUDValue6);
						Memory::Write(Marbella_ChaletsHUDScansResult[Marbella_ChalettsHUD7] + 1, m_newMarbella_ChaletsHUDValue7);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "marbella_f01.dll")) // Marbella Part 1
				{
					auto Marbella_F01HUDScanResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24");
					if (Marbella_F01HUDScanResult)
					{
						spdlog::info("HUD Instruction: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F01HUDScanResult - (std::uint8_t*)module);

						Memory::Write(Marbella_F01HUDScanResult + 1, m_newHUDWidth);
					}
					else
					{
						spdlog::info("Cannot locate the HUD instruction memory address (Marbella_f01.dll).");
						return;
					}
				}

				if (Util::stringcmp_caseless(moduleName, "marbella_f02.dll")) // Marbella Part 2
				{
					auto Marbella_F02HUDInstructionsScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF 15",
					"68 ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 8B 3D", "68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? FF D7" "68 ?? ?? ?? ?? 8D 84 24 ?? ?? ?? ?? 50",
					"68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 68" "68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A ?? 89 4C 24",
					"68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 6A",
					"68 ?? ?? ?? ?? FF D7 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 94 24 ?? ?? ?? ?? 52 8D 84 24 ?? ?? ?? ?? 50 6A ?? 68");
					if (Memory::AreAllSignaturesValid(Marbella_F02HUDInstructionsScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD6] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 7: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD7] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 8: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD8] - (std::uint8_t*)module);

						Memory::Write(Marbella_F02HUDInstructionsScansResult, Marbella_F02HUD1, Marbella_F02HUD6, 1, m_newHUDWidth);

						m_newMarbella_F02HUDValue7 = (int)(std::round(600.0f * m_newAspectRatio - 109.0f));
						m_newMarbella_F02HUDValue8 = (int)(std::round(600.0f * m_newAspectRatio - 149.0f));
						Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD7] + 1, m_newMarbella_F02HUDValue7);						
						Memory::Write(Marbella_F02HUDInstructionsScansResult[Marbella_F02HUD8] + 1, m_newMarbella_F02HUDValue8);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "marbella_f03.dll")) // Marbella Part 3
				{
					auto Marbella_F03HUDScanResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24");
					if (Marbella_F03HUDScanResult)
					{
						spdlog::info("HUD Instruction: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F03HUDScanResult - (std::uint8_t*)module);

						Memory::Write(Marbella_F03HUDScanResult + 6, m_newHUDWidth);
					}
					else
					{
						spdlog::info("Cannot locate the HUD instruction memory address (Marbella_f03.dll).");
						return;
					}
				}

				if (Util::stringcmp_caseless(moduleName, "marbella_f04.dll")) // Marbella Part 4
				{
					auto Marbella_F04HUDScanResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 94 24");
					if (Marbella_F04HUDScanResult)
					{
						spdlog::info("HUD Instruction: Address is {:s}+{:x}", moduleName.c_str(), Marbella_F04HUDScanResult - (std::uint8_t*)module);

						Memory::Write(Marbella_F04HUDScanResult + 1, m_newHUDWidth);
					}
					else
					{
						spdlog::info("Cannot locate the HUD instruction memory address (Marbella_f04.dll).");
						return;
					}
				}

				if (Util::stringcmp_caseless(moduleName, "marbella_malibu.dll")) // Marbella Malibu Mansion Level
				{
					auto Marbella_MalibuHUDScanResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8D 8C 24");
					if (Marbella_MalibuHUDScanResult)
					{
						spdlog::info("HUD Instruction: Address is {:s}+{:x}", moduleName.c_str(), Marbella_MalibuHUDScanResult - (std::uint8_t*)module);

						Memory::Write(Marbella_MalibuHUDScanResult + 1, m_newHUDWidth);
					}
					else
					{
						spdlog::info("Cannot locate the HUD instruction memory address (marbella_malibu.dll).");
						return;
					}
				}

				if (Util::stringcmp_caseless(moduleName, "radar.dll")) // Radar HUD
				{
					auto RadarHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D6 68",
					"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 44 24", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 6A ?? FF D6",
					"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A ?? 6A ?? FF D6 B8", "68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24 ?? 50 6A ?? 6A ?? FF 15",
					"68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 8D 84 24");
					if (Memory::AreAllSignaturesValid(RadarHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), RadarHUDScansResult[RadarHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), RadarHUDScansResult[RadarHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), RadarHUDScansResult[RadarHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), RadarHUDScansResult[RadarHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), RadarHUDScansResult[RadarHUD5] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 6: Address is {:s}+{:x}", moduleName.c_str(), RadarHUDScansResult[RadarHUD6] - (std::uint8_t*)module);

						Memory::Write(RadarHUDScansResult, RadarHUD1, RadarHUD6, 6, m_newHUDWidth);
					}
				}

				if (Util::stringcmp_caseless(moduleName, "script.dll")) // Script HUD
				{
					auto ScriptHUDScansResult = Memory::PatternScan(module, "68 ?? ?? ?? ?? 8B F9 8D 44 24 ?? 50 8D 4C 24 ?? 51 68 ?? ?? ?? ?? 68 ?? ?? ?? ?? 89 7C 24 ?? FF 15 ?? ?? ?? ?? 83 C4 ?? BD",
					"68 ?? ?? ?? ?? 8B D9", "68 ?? ?? ?? ?? 8D 44 24 ?? 50 8D 4C 24 ?? 51 6A", "68 ?? ?? ?? ?? 8D 54 24 ?? 52 8D 44 24", "68 ?? ?? ?? ?? 8D 4C 24 ?? 51 8D 54 24 ?? 52 6A");
					if (Memory::AreAllSignaturesValid(ScriptHUDScansResult) == true)
					{
						spdlog::info("HUD Instruction 1: Address is {:s}+{:x}", moduleName.c_str(), ScriptHUDScansResult[ScriptHUD1] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 2: Address is {:s}+{:x}", moduleName.c_str(), ScriptHUDScansResult[ScriptHUD2] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 3: Address is {:s}+{:x}", moduleName.c_str(), ScriptHUDScansResult[ScriptHUD3] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 4: Address is {:s}+{:x}", moduleName.c_str(), ScriptHUDScansResult[ScriptHUD4] - (std::uint8_t*)module);
						spdlog::info("HUD Instruction 5: Address is {:s}+{:x}", moduleName.c_str(), ScriptHUDScansResult[ScriptHUD5] - (std::uint8_t*)module);

						Memory::Write(ScriptHUDScansResult, ScriptHUD1, ScriptHUD5, 1, m_newHUDWidth);
					}
				}
			},
			// OnUnload
			[this](HMODULE module)
			{
				const std::string moduleName = Memory::GetModuleName(module);

				if (IsWatchedModule(moduleName))
				{
					spdlog::info("{:s} unloaded.", moduleName.c_str());
				}
			});

			if (!m_gameWatcher->Start())
			{
				spdlog::error("DllNotificationWatcher failed. NTSTATUS=0x{:08X}, Win32Error={}.", static_cast<unsigned long>(m_gameWatcher->LastNtStatus()), m_gameWatcher->LastWin32Error());
				m_gameWatcher.reset();
				return;
			}
	}

private:
	static constexpr float m_oldAspectRatio = 4.0f / 3.0f;
	static constexpr float m_defaultCameraFOV = 0.6899999976f;

	SafetyHookMid m_sniperZoomFOVHook{};

	HMODULE m_vtKernelDllModule = nullptr;
	std::string m_vtKernelDllModuleName = "";

	enum ResolutionInstructionsIndices
	{
		List1,
		List2
	};

	enum CameraFOVInstructionsIndices
	{
		ThirdPerson,
		FirstPerson1,
		FirstPerson2,
		SniperZoomIn,
		SniperZoomOut
	};

	enum CocheTorrenteHUDInstructionsIndices
	{
		CocheTorrenteHUD1,
		CocheTorrenteHUD2,
		CocheTorrenteHUD3,
		CocheTorrenteHUD4,
		CocheTorrenteHUD5,
		CocheTorrenteHUD6,
		CocheTorrenteHUD7,
		CocheTorrenteHUD8,
		CocheTorrenteHUD9,
		CocheTorrenteHUD10,
		CocheTorrenteHUD11
	};

	enum ComercialHUDInstructionsIndices
	{
		ComercialHUD1,
		ComercialHUD2,
		ComercialHUD3,
		ComercialHUD4,
		ComercialHUD5,
		ComercialHUD6,
		ComercialHUD7
	};

	enum ContadorDetonadorHUDInstructionsIndices
	{
		ContadorDetonadorHUD1,
		ContadorDetonadorHUD2,
		ContadorDetonadorHUD3,
		ContadorDetonadorHUD4,
		ContadorDetonadorHUD5,
		ContadorDetonadorHUD6
	};

	enum ContadorDineroHUDInstructionsIndices
	{
		ContadorDineroHUD1,
		ContadorDineroHUD2,
		ContadorDineroHUD3,
		ContadorDineroHUD4,
		ContadorDineroHUD5,
		ContadorDineroHUD6
	};

	enum ContadorMunicionHUDInstructionsIndices
	{
		ContadorMunicionHUD1,
		ContadorMunicionHUD2,
		ContadorMunicionHUD3,
		ContadorMunicionHUD4
	};

	enum ContadorSaludHUDInstructionsIndices
	{
		ContadorSaludHUD1,
		ContadorSaludHUD2,
		ContadorSaludHUD3,
		ContadorSaludHUD4,
		ContadorSaludHUD5,
		ContadorSaludHUD6,
		ContadorSaludHUD7,
		ContadorSaludHUD8
	};

	enum ContadorTiempoHUDInstructionsIndices
	{
		ContadorTiempoHUD1,
		ContadorTiempoHUD2,
		ContadorTiempoHUD3,
		ContadorTiempoHUD4,
		ContadorTiempoHUD5,
		ContadorTiempoHUD6,
		ContadorTiempoHUD7
	};

	enum FaseCucoHUDInstructionsIndices
	{
		FaseCucoHUD1,
		FaseCucoHUD2,
		FaseCucoHUD3,
		FaseCucoHUD4,
		FaseCucoHUD5,
		FaseCucoHUD6,
		FaseCucoHUD7,
		FaseCucoHUD8,
		FaseCucoHUD9,
		FaseCucoHUD10,
		FaseCucoHUD11,
		FaseCucoHUD12,
		FaseCucoHUD13,
		FaseCucoHUD14,
		FaseCucoHUD15
	};

	enum Madrid_F03HUDInstructionsIndices
	{
		Madrid_F03HUD1,
		Madrid_F03HUD2,
		Madrid_F03HUD3,
		Madrid_F03HUD4,
		Madrid_F03HUD5,
		Madrid_F03HUD6
	};

	enum Madrid_F04HUDInstructionsIndices
	{
		Madrid_F04HUD1,
		Madrid_F04HUD2,
		Madrid_F04HUD3,
		Madrid_F04HUD4
	};

	enum MadridFrancoHUDInstructionsIndices
	{
		Madrid_FrancoHUD1,
		Madrid_FrancoHUD2,
		Madrid_FrancoHUD3,
		Madrid_FrancoHUD4,
		Madrid_FrancoHUD5,
		Madrid_FrancoHUD6,
		Madrid_FrancoHUD7,
		Madrid_FrancoHUD8,
		Madrid_FrancoHUD9,
		Madrid_FrancoHUD10,
		Madrid_FrancoHUD11,
		Madrid_FrancoHUD12,
		Madrid_FrancoHUD13,
		Madrid_FrancoHUD14,
		Madrid_FrancoHUD15,
		Madrid_FrancoHUD16,
		Madrid_FrancoHUD17,
		Madrid_FrancoHUD18,
		Madrid_FrancoHUD19,
		Madrid_FrancoHUD20,
		Madrid_FrancoHUD21,
		Madrid_FrancoHUD22,
		Madrid_FrancoHUD23,
		Madrid_FrancoHUD24,
		Madrid_FrancoHUD25,
		Madrid_FrancoHUD26,
		Madrid_FrancoHUD27,
	};

	enum MainHUDInstructionsIndices
	{
		MainHUD1,
		MainHUD2,
		MainHUD3,
		MainHUD4
	};

	enum Marbella_ChalettsHUDInstructionsIndices
	{
		Marbella_ChalettsHUD1,
		Marbella_ChalettsHUD2,
		Marbella_ChalettsHUD3,
		Marbella_ChalettsHUD4,
		Marbella_ChalettsHUD5,
		Marbella_ChalettsHUD6,
		Marbella_ChalettsHUD7
	};

	enum Marbella_F02HUDInstructionsIndices
	{
		Marbella_F02HUD1,
		Marbella_F02HUD2,
		Marbella_F02HUD3,
		Marbella_F02HUD4,
		Marbella_F02HUD5,
		Marbella_F02HUD6,
		Marbella_F02HUD7,
		Marbella_F02HUD8
	};

	enum RadarHUDInstructionsIndices
	{
		RadarHUD1,
		RadarHUD2,
		RadarHUD3,
		RadarHUD4,
		RadarHUD5,
		RadarHUD6
	};

	enum ScriptHUDInstructionsIndices
	{
		ScriptHUD1,
		ScriptHUD2,
		ScriptHUD3,
		ScriptHUD4,
		ScriptHUD5,
		ScriptHUD6
	};

	float m_firstPersonFOVFactor = 0.0f;
	float m_thirdPersonFOVFactor = 0.0f;
	float m_zoomFactor = 0.0f;
	float m_newThirdPersonFOV = 0.0f;
	float m_newFirstPersonFOV = 0.0f;
	float m_newSniperZoomInFOV = 0.0f;
	float m_newSniperZoomOutFOV = 0.0f;

	int m_newHUDWidth = 0;
	int m_newCocheTorrenteHUDValue10 = 0;
	float m_newCocheTorrenteHUDFOV = 0.0f;		
	int m_newComercialHUDValue6 = 0;
	int m_newComercialHUDValue7 = 0;
	int m_newContadorDetonadorHUDValue5 = 0;
	int m_newContadorDetonadorHUDValue6 = 0;
	int m_newContadorDineroHUDValue5 = 0;
	int m_newContadorDineroHUDValue6 = 0;
	int m_newContadorTiempoHUDValue6 = 0;
	int m_newContadorTiempoHUDValue7 = 0;
	int m_newFaseCucoHUDValue12 = 0;
	int m_newFaseCucoHUDValue13 = 0;
	int m_newFaseCucoHUDValue14 = 0;
	int m_newFaseCucoHUDValue15 = 0;
	int m_newMadrid_F03HUDValue5 = 0;
	int m_newMadrid_F03HUDValue6 = 0;
	int m_newMadrid_F04HUDValue4 = 0;
	int m_newMadrid_FrancoHUDValue25 = 0;
	int m_newMadrid_FrancoHUDValue26 = 0;
	int m_newMadrid_FrancoHUDValue27 = 0;
	int m_newMarbella_ChaletsHUDValue6 = 0;
	int m_newMarbella_ChaletsHUDValue7 = 0;
	int m_newMarbella_F02HUDValue7 = 0;
	int m_newMarbella_F02HUDValue8 = 0;

	static bool IsWatchedModule(const std::string& moduleName)
	{
		static constexpr std::array<const char*, 23> watchedModules =
		{
			"camera.dll",
			"cochetorrente.dll",
			"comercial.dll",
			"contadordetonador.dll",
			"contadordinero.dll",
			"contadormunicion.dll",
			"contadorsalud.dll",
			"contadortiempo.dll",
			"fase_cuco.dll",
			"fase_spinelli.dll",
			"madrid_f03.dll",
			"madrid_f04.dll",
			"madrid_franco.dll",
			"madrid_kio.dll",
			"main.dll",
			"marbella_chalets.dll",
			"marbella_f01.dll",
			"marbella_f02.dll",
			"marbella_f03.dll",
			"marbella_f04.dll",
			"marbella_malibu.dll",
			"radar.dll",
			"script.dll"
		};

		for (const char* watchedModule : watchedModules)
		{
			if (Util::stringcmp_caseless(moduleName, watchedModule))
			{
				return true;
			}
		}

		return false;
	}


	std::unique_ptr<DllNotificationWatcher> m_gameWatcher;

	inline static Torrente1Fix* s_instance_ = nullptr;
};

static std::unique_ptr<Torrente1Fix> g_fix;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		DisableThreadLibraryCalls(hModule);

		g_fix = std::make_unique<Torrente1Fix>(hModule);
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