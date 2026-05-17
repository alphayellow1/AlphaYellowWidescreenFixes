#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winternl.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace DllNotificationDetail
{
    constexpr ULONG LdrDllNotificationReasonLoaded = 1;
    constexpr ULONG LdrDllNotificationReasonUnloaded = 2;

    struct LdrDllLoadedNotificationData
    {
        ULONG Flags;
        PCUNICODE_STRING FullDllName;
        PCUNICODE_STRING BaseDllName;
        PVOID DllBase;
        ULONG SizeOfImage;
    };

    struct LdrDllUnloadedNotificationData
    {
        ULONG Flags;
        PCUNICODE_STRING FullDllName;
        PCUNICODE_STRING BaseDllName;
        PVOID DllBase;
        ULONG SizeOfImage;
    };

    union LdrDllNotificationData
    {
        LdrDllLoadedNotificationData Loaded;
        LdrDllUnloadedNotificationData Unloaded;
    };

    using LdrDllNotificationFunction = VOID(CALLBACK*)(
        ULONG NotificationReason,
        const LdrDllNotificationData* NotificationData,
        PVOID Context
        );

    using LdrRegisterDllNotificationFn = NTSTATUS(NTAPI*)(
        ULONG Flags,
        LdrDllNotificationFunction NotificationFunction,
        PVOID Context,
        PVOID* Cookie
        );

    using LdrUnregisterDllNotificationFn = NTSTATUS(NTAPI*)(
        PVOID Cookie
        );
}

class DllNotificationWatcher final
{
public:
    using ModuleCallback = std::function<void(HMODULE module)>;

    DllNotificationWatcher(
        ModuleCallback onLoad,
        ModuleCallback onUnload
    );

    ~DllNotificationWatcher();

    DllNotificationWatcher(const DllNotificationWatcher&) = delete;
    DllNotificationWatcher& operator=(const DllNotificationWatcher&) = delete;

    DllNotificationWatcher(DllNotificationWatcher&&) = delete;
    DllNotificationWatcher& operator=(DllNotificationWatcher&&) = delete;

    bool Start();
    void Stop();

    bool IsRunning() const;

    NTSTATUS LastNtStatus() const;
    DWORD LastWin32Error() const;

private:
    enum class EventType
    {
        Load,
        Unload
    };

    struct PendingEvent
    {
        EventType Type;
        HMODULE Module;
    };

    static VOID CALLBACK NotificationCallback(
        ULONG notificationReason,
        const DllNotificationDetail::LdrDllNotificationData* notificationData,
        PVOID context
    );

    bool RegisterNotification();
    void UnregisterNotification();

    void QueueEvent(EventType type, HMODULE module);
    void WorkerLoop();

private:
    ModuleCallback m_onLoad;
    ModuleCallback m_onUnload;

    PVOID m_cookie = nullptr;
    HANDLE m_event = nullptr;

    std::atomic_bool m_running = false;

    std::thread m_workerThread;

    std::mutex m_mutex;
    std::vector<PendingEvent> m_pendingEvents;

    NTSTATUS m_lastNtStatus = 0;
    DWORD m_lastWin32Error = ERROR_SUCCESS;
};