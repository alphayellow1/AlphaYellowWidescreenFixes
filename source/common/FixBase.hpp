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
    explicit FixBase(HMODULE selfModule) : thisModule_(selfModule), exeModule_(GetModuleHandleW(nullptr))
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
        if (logger_)
        {
            logger_->flush();
            spdlog::drop(FixName());
            logger_.reset();
        }
    }

    void Initialize()
    {
        Logging();
        LoadConfiguration();

        if (enabled_ == false)
        {
            spdlog::info("Fix disabled in config.");
            return;
        }

        if (!IsCompatibleExecutable(exeName_))
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
        spdlog::error("Executable '{}' is not supported by the fix.", exeName_);
    }

    HMODULE SelfModule() const
    {
        return thisModule_;
    }

    HMODULE ExeModule() const
    {
        return exeModule_;
    }

    const std::filesystem::path& FixPath() const
    {
        return fixPath_;
    }

    const std::filesystem::path& ExePath() const
    {
        return exePath_;
    }

    const std::string& ExeName() const
    {
        return exeName_;
    }

    std::shared_ptr<spdlog::logger> Logger() const
    {
        return logger_;
    }

    bool enabled_ = true;

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
        GetModuleFileNameW(thisModule_, dllPath, MAX_PATH);
        fixPath_ = std::filesystem::path(dllPath).remove_filename();

        WCHAR exePathW[MAX_PATH] = {};
        GetModuleFileNameW(exeModule_, exePathW, MAX_PATH);
        exePath_ = std::filesystem::path(exePathW).remove_filename();
        exeName_ = std::filesystem::path(exePathW).filename().string();

        logFile_ = std::string(FixName()) + ".log";

        try
        {
            logger_ = spdlog::basic_logger_st(FixName(), (exePath_ / logFile_).string(), true);
            spdlog::set_default_logger(logger_);
            spdlog::flush_on(spdlog::level::debug);
            spdlog::set_level(spdlog::level::debug);

            spdlog::info("----------");
            spdlog::info("{} v{} loaded.", FixName(), FixVersion());
            spdlog::info("----------");
            spdlog::info("Log file: {}", (exePath_ / logFile_).string());
            spdlog::info("Module Name: {}", exeName_);
            spdlog::info("Module Path: {}", exePath_.string());
            spdlog::info("Module Address: 0x{:X}", reinterpret_cast<uintptr_t>(exeModule_));
            spdlog::info("----------");
        }
        catch (const spdlog::spdlog_ex& ex)
        {
            AllocConsole();
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << "Log initialization failed: " << ex.what() << std::endl;
            FreeLibraryAndExitThread(thisModule_, 1);
        }
    }

    virtual void LoadConfiguration()
    {
        configFile_ = ConfigFileName();

        std::ifstream iniFile((fixPath_ / configFile_).string());
        if (!iniFile)
        {
            AllocConsole();
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            std::cout << FixName() << " v" << FixVersion() << " loaded.\n";
            std::cout << "ERROR: Could not locate config file.\n";
            std::cout << "ERROR: Make sure " << configFile_ << " is located in " << fixPath_.string() << "\n";
            spdlog::shutdown();
            FreeLibraryAndExitThread(thisModule_, 1);
        }

        spdlog::info("Config file: {}", (fixPath_ / configFile_).string());

        ini_.parse(iniFile);
        ini_.strip_trailing_comments();

        spdlog::info("----------");

        inipp::get_value(ini_.sections["Fix"], "Enabled", enabled_);

        ParseFixConfig(ini_);

        spdlog::info("----------");
    }

private:
    HMODULE thisModule_ = nullptr;
    HMODULE exeModule_ = nullptr;

    std::filesystem::path fixPath_;
    std::filesystem::path exePath_;

    std::string exeName_;
    std::string configFile_;
    std::string logFile_;

    inipp::Ini<char> ini_;
    std::shared_ptr<spdlog::logger> logger_;
};