#pragma once

#include <abel/Error.hpp>
#include <abel/Handle.hpp>
#include <abel/Thread.hpp>

#include <Windows.h>
#include <utility>
#include <concepts>
#include <optional>

namespace abel {

template <typename T>
    //requires std::derived_from<T, Service<T>>
class Service {
protected:
    static std::optional<T> instance;

    SERVICE_STATUS status{
        .dwServiceType = SERVICE_WIN32_OWN_PROCESS,
        .dwCurrentState = SERVICE_START_PENDING,
        .dwControlsAccepted = 0,
        .dwWin32ExitCode = 0,
        .dwServiceSpecificExitCode = 0,
        .dwCheckPoint = 0,
    };
    SERVICE_STATUS_HANDLE status_handle{};
    OwningHandle stop_event{};
    int argc{};
    const char **argv{};

    Service() {
    }

    static void init(int argc, const char **argv) {
        get().init_(argc, argv);
    }

    void init_(int argc_, const char **argv_) {
        try {
            argc = argc_;
            argv = argv_;
            status_handle = RegisterServiceCtrlHandlerA(T::name, &control_handler);

            if (!status_handle) {
                fail("Failed to register service control handler");
            }

            report_status(SERVICE_START_PENDING);

            stop_event = Handle::create_event();

            report_status(SERVICE_RUNNING);

            OwningHandle thread = Thread::create<T, &T::work>(&instance.value()).handle;

            Handle::wait_multiple(thread, stop_event);

            // Probably not necessary, as the kernel will clean up all our threads anyway
            // thread.terminate_thread();

            report_status(SERVICE_STOPPED, 0);
        } catch (std::exception &e) {
            log<EVENTLOG_ERROR_TYPE>("ERROR! %s", e.what());
            report_status(SERVICE_STOPPED, 1);
        }
    }

    static void control_handler(DWORD control) {
        get().control_handler_(control);
    }

    void control_handler_(DWORD control) {
        try {
            switch (control) {
            case SERVICE_CONTROL_STOP:
                report_status(SERVICE_STOP_PENDING);
                stop_event.signal();
                report_status(status.dwCurrentState);
                return;

            case SERVICE_CONTROL_INTERROGATE:
                break;

            default:
                break;
            }
        } catch (std::exception &e) {
            log<EVENTLOG_ERROR_TYPE>("ERROR processing control %u! %s", control, e.what());
        }
    }

    void report_status(DWORD state, DWORD exitCode = 0, DWORD waitHint = 0) {
        status.dwCurrentState = state;
        status.dwWin32ExitCode = exitCode;
        status.dwWaitHint = waitHint;

        if (state == SERVICE_START_PENDING) {
            status.dwControlsAccepted = 0;
        } else {
            status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
        }

        if ((state == SERVICE_RUNNING) || (state == SERVICE_STOPPED)) {
            status.dwCheckPoint = 0;
        } else {
            status.dwCheckPoint++;
        }

        bool success = SetServiceStatus(status_handle, &status);
        if (!success) {
            fail("Failed to report service status");
        }
    }

public:
    Service(const Service &) = delete;
    Service &operator=(const Service &) = delete;

    constexpr Service(Service &&) noexcept = default;
    constexpr Service &operator=(Service &&) noexcept = default;

    ~Service() = default;

    static T &get() {
        if (!instance) {
            fail("Service not initialized");
        }

        return instance.value();
    }

    static void startup() {
        instance = T();

        std::string name{T::name};

        SERVICE_TABLE_ENTRYA service_table[] = {
            {name.data(), (LPSERVICE_MAIN_FUNCTIONA)&init},
            {    nullptr,                         nullptr}
        };

        StartServiceCtrlDispatcherA(service_table);
    }

    template <DWORD kind = EVENTLOG_INFORMATION_TYPE, typename... Args>
    void log(const char *message, Args... args) {
        auto eventSource = Handle{RegisterEventSourceA(nullptr, T::name)};

        char buf[4096] = {};
        int offset = sprintf_s(buf, sizeof(buf), "%s: (err=%d wsaerr=%d) -- ", T::name, GetLastError(), WSAGetLastError());
        sprintf_s(buf + offset, sizeof(buf) - offset, message, args...);

        const char *pbuf = buf;

        ReportEventA(
            eventSource.raw(),  // event log handle
            kind,               // event type
            0,                  // event category
            0,                  // event identifier
            nullptr,            // no security identifier
            1,                  // size of lpszStrings array
            0,                  // no binary data
            &pbuf,              // array of strings
            nullptr             // no binary data
        );

        DeregisterEventSource(eventSource.raw());
    }
};

template <typename T>
std::optional<T> Service<T>::instance = std::nullopt;

}  // namespace abel
