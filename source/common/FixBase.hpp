#pragma once

#include "stdafx.h"
#include "helper.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <inipp/inipp.h>
#include <safetyhook.hpp>

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <cstdint>

#define spdlog_confparse(var) spdlog::info("Config Parse: {}: {}", #var, var)

class FixBase
{
public:
    explicit FixBase(HMODULE selfModule) : m_thisModule(selfModule), m_exeModule(GetModuleHandleW(nullptr))
    {
    }

    virtual ~FixBase() = default;

    bool Start()
    {
        switch (GetInitMode())
        {
        case InitMode::WorkerThread:
            return StartWorkerThread();

        case InitMode::Direct:
            Initialize();
            return true;

        case InitMode::ExportedOnly:
            return true;
        }

        return false;
    }

    virtual void Shutdown()
    {
        if (m_logger)
        {
            m_logger->flush();
            spdlog::drop(FixName());
            m_logger.reset();
        }
    }

    void Initialize()
    {
        Logging();
        LoadConfiguration();

        if (m_enabled == false)
        {
            spdlog::info("Fix disabled in config.");
            return;
        }

        if (!IsCompatibleExecutable(m_exeName))
        {
            OnIncompatibleExecutable();
            return;
        }

        OnBeforeApply();
        ApplyFix();
        OnAfterApply();
    }

protected:
    float m_fovFactor = 1.0f;
    int m_newResX = 0;
    int m_newResY = 0;
    float m_newAspectRatio = 0.0f;
    float m_aspectRatioScale = 1.0f;
    float m_newCameraFOV = 0.0f;
    float m_newAspectRatio2 = 0.0f;

    enum class InitMode
    {
        WorkerThread,
        Direct,
        ExportedOnly
    };

    virtual const char* FixName() const = 0;
    virtual const char* FixVersion() const = 0;

    virtual std::string ConfigFileName() const
    {
        return std::string(FixName()) + ".ini";
    }

    virtual InitMode GetInitMode() const
    {
        return InitMode::WorkerThread;
    }

    virtual bool IsCompatibleExecutable(const std::string& exeName) const = 0;

    virtual const char* TargetName() const
    {
        return "Unknown Target";
    }

    virtual void ParseFixConfig(inipp::Ini<char>& ini) = 0;

    virtual void ApplyFix() = 0;

    virtual void OnBeforeApply() {}
    virtual void OnAfterApply() {}

    virtual void OnIncompatibleExecutable()
    {
        spdlog::error("Executable '{}' is not supported by the fix.", m_exeName);
    }

    HMODULE SelfModule() const
    {
        return m_thisModule;
    }

    HMODULE ExeModule() const
    {
        return m_exeModule;
    }

    const std::filesystem::path& FixPath() const
    {
        return m_fixPath;
    }

    const std::filesystem::path& ExePath() const
    {
        return m_exePath;
    }

    const std::string& ExeName() const
    {
        return m_exeName;
    }

    std::shared_ptr<spdlog::logger> Logger() const
    {
        return m_logger;
    }

    bool m_enabled = true;

    virtual void FallbackToDesktopResolution(int& width, int& height)
    {
        if (width <= 0 || height <= 0)
        {
            spdlog::info("Resolution not specified in ini file. Using desktop resolution.");

            auto desktopDimensions = Util::GetPhysicalDesktopDimensions();
            width = desktopDimensions.first;
            height = desktopDimensions.second;
        }
    }

private:
    static DWORD WINAPI ThreadProc(LPVOID param)
    {
        auto* self = static_cast<FixBase*>(param);
        self->Initialize();
        return 0;
    }

    bool StartWorkerThread()
    {
        HANDLE thread = CreateThread(nullptr, 0, &FixBase::ThreadProc, this, 0, nullptr);
        if (!thread)
        {
            return false;
        }

        SetThreadPriority(thread, THREAD_PRIORITY_HIGHEST);
        CloseHandle(thread);
        return true;
    }

    virtual void Logging()
    {
        WCHAR dllPath[MAX_PATH] = {};
        GetModuleFileNameW(m_thisModule, dllPath, MAX_PATH);
        m_fixPath = std::filesystem::path(dllPath).remove_filename();

        WCHAR exePathW[MAX_PATH] = {};
        GetModuleFileNameW(m_exeModule, exePathW, MAX_PATH);
        m_exePath = std::filesystem::path(exePathW).remove_filename();
        m_exeName = std::filesystem::path(exePathW).filename().string();

        m_logFile = std::string(FixName()) + ".log";

        try
        {
            m_logger = spdlog::basic_logger_st(FixName(), (m_exePath / m_logFile).string(), true);
            spdlog::set_default_logger(m_logger);
            spdlog::flush_on(spdlog::level::debug);
            spdlog::set_level(spdlog::level::debug);

            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", FixName(), FixVersion());
            spdlog::info("----------");
            spdlog::info("Log file: {}", (m_exePath / m_logFile).string());
            spdlog::info("Module Name: {}", m_exeName);
            spdlog::info("Module Path: {}", m_exePath.string());
            spdlog::info("Module Address: 0x{:X}", reinterpret_cast<uintptr_t>(m_exeModule));
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            AllocConsole();
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialization failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(m_thisModule, 1);
        }
    }

    virtual void LoadConfiguration()
    {
        m_configFile = ConfigFileName();

        std::ifstream iniFile((m_fixPath / m_configFile).string());
        if (!iniFile)
        {
            AllocConsole();
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << FixName() << " v" << FixVersion() << " loaded.\n";
            std::cout << "ERROR: Could not locate config file.\n";
            std::cout << "ERROR: Make sure " << m_configFile << " is located in " << m_fixPath.string() << "\n";
            spdlog::shutdown();
            FreeLibraryAndExitThread(m_thisModule, 1);
        }

        spdlog::info("Config file: {}", (m_fixPath / m_configFile).string());

        m_ini.parse(iniFile);
        m_ini.strip_trailing_comments();

        spdlog::info("----------");

        inipp::get_value(m_ini.sections["Fix"], "Enabled", m_enabled);

        ParseFixConfig(m_ini);

        spdlog::info("----------");
    }

private:
    HMODULE m_thisModule = nullptr;
    HMODULE m_exeModule = nullptr;

    std::filesystem::path m_fixPath;
    std::filesystem::path m_exePath;
    std::string m_exeName;
    std::string m_configFile;
    std::string m_logFile;

    inipp::Ini<char> m_ini;
    std::shared_ptr<spdlog::logger> m_logger;
};