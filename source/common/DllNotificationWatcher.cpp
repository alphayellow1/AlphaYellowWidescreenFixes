#include "DllNotificationWatcher.hpp"

DllNotificationWatcher::DllNotificationWatcher(
    ModuleCallback onLoad,
    ModuleCallback onUnload
)
    : m_onLoad(std::move(onLoad))
    , m_onUnload(std::move(onUnload))
{
}

DllNotificationWatcher::~DllNotificationWatcher()
{
    Stop();
}

bool DllNotificationWatcher::Start()
{
    if (m_running.load(std::memory_order_acquire))
    {
        return true;
    }

    m_lastNtStatus = 0;
    m_lastWin32Error = ERROR_SUCCESS;

    m_event = CreateEventW(
        nullptr,
        FALSE,
        FALSE,
        nullptr
    );

    if (!m_event)
    {
        m_lastWin32Error = GetLastError();
        return false;
    }

    m_running.store(true, std::memory_order_release);

    m_workerThread = std::thread([this]()
        {
            WorkerLoop();
        });

    if (!RegisterNotification())
    {
        Stop();
        return false;
    }

    return true;
}

void DllNotificationWatcher::Stop()
{
    UnregisterNotification();

    if (m_running.exchange(false, std::memory_order_acq_rel))
    {
        if (m_event)
        {
            SetEvent(m_event);
        }
    }

    if (m_workerThread.joinable())
    {
        m_workerThread.join();
    }

    if (m_event)
    {
        CloseHandle(m_event);
        m_event = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingEvents.clear();
    }
}

bool DllNotificationWatcher::IsRunning() const
{
    return m_running.load(std::memory_order_acquire);
}

NTSTATUS DllNotificationWatcher::LastNtStatus() const
{
    return m_lastNtStatus;
}

DWORD DllNotificationWatcher::LastWin32Error() const
{
    return m_lastWin32Error;
}

bool DllNotificationWatcher::RegisterNotification()
{
    HMODULE ntdllModule = GetModuleHandleW(L"ntdll.dll");

    if (!ntdllModule)
    {
        m_lastWin32Error = GetLastError();
        return false;
    }

    auto ldrRegisterDllNotification =
        reinterpret_cast<DllNotificationDetail::LdrRegisterDllNotificationFn>(
            GetProcAddress(ntdllModule, "LdrRegisterDllNotification")
            );

    if (!ldrRegisterDllNotification)
    {
        m_lastWin32Error = GetLastError();
        return false;
    }

    NTSTATUS status = ldrRegisterDllNotification(
        0,
        NotificationCallback,
        this,
        &m_cookie
    );

    m_lastNtStatus = status;

    if (!NT_SUCCESS(status))
    {
        m_cookie = nullptr;
        return false;
    }

    return true;
}

void DllNotificationWatcher::UnregisterNotification()
{
    if (!m_cookie)
    {
        return;
    }

    HMODULE ntdllModule = GetModuleHandleW(L"ntdll.dll");

    if (!ntdllModule)
    {
        m_cookie = nullptr;
        return;
    }

    auto ldrUnregisterDllNotification =
        reinterpret_cast<DllNotificationDetail::LdrUnregisterDllNotificationFn>(
            GetProcAddress(ntdllModule, "LdrUnregisterDllNotification")
            );

    if (ldrUnregisterDllNotification)
    {
        m_lastNtStatus = ldrUnregisterDllNotification(m_cookie);
    }

    m_cookie = nullptr;
}

VOID CALLBACK DllNotificationWatcher::NotificationCallback(
    ULONG notificationReason,
    const DllNotificationDetail::LdrDllNotificationData* notificationData,
    PVOID context
)
{
    auto* self = static_cast<DllNotificationWatcher*>(context);

    if (!self || !notificationData)
    {
        return;
    }

    if (notificationReason == DllNotificationDetail::LdrDllNotificationReasonLoaded)
    {
        self->QueueEvent(
            EventType::Load,
            reinterpret_cast<HMODULE>(notificationData->Loaded.DllBase)
        );
    }
    else if (notificationReason == DllNotificationDetail::LdrDllNotificationReasonUnloaded)
    {
        self->QueueEvent(
            EventType::Unload,
            reinterpret_cast<HMODULE>(notificationData->Unloaded.DllBase)
        );
    }
}

void DllNotificationWatcher::QueueEvent(EventType type, HMODULE module)
{
    if (!module)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_pendingEvents.push_back(
            PendingEvent
            {
                type,
                module
            }
        );
    }

    if (m_event)
    {
        SetEvent(m_event);
    }
}

void DllNotificationWatcher::WorkerLoop()
{
    while (m_running.load(std::memory_order_acquire))
    {
        DWORD waitResult = WaitForSingleObject(m_event, INFINITE);

        if (!m_running.load(std::memory_order_acquire))
        {
            break;
        }

        if (waitResult != WAIT_OBJECT_0)
        {
            continue;
        }

        std::vector<PendingEvent> events;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            events.swap(m_pendingEvents);
        }

        for (const PendingEvent& event : events)
        {
            try
            {
                if (event.Type == EventType::Load)
                {
                    if (m_onLoad)
                    {
                        m_onLoad(event.Module);
                    }
                }
                else
                {
                    if (m_onUnload)
                    {
                        m_onUnload(event.Module);
                    }
                }
            }
            catch (...)
            {
                // Never allow user callbacks to kill the worker thread.
            }
        }
    }
}