#include <abel/Thread.hpp>

#include <abel/Error.hpp>

namespace abel {

Thread Thread::create(
    LPTHREAD_START_ROUTINE func,
    void *param,
    bool inheritHandles,
    bool startSuspended
) {
    Thread result{};

    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = inheritHandles,
    };

    DWORD flags = 0;

    if (startSuspended) {
        flags |= CREATE_SUSPENDED;
    }

    result.handle = OwningHandle(CreateThread(&sa, 0, func, param, flags, &result.tid)).validate();

    return result;
}

Thread Thread::create_remote(
    Handle process,
    LPTHREAD_START_ROUTINE func,
    void *param,
    bool inheritHandles,
    bool startSuspended
) {
    Thread result{};

    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = inheritHandles,
    };

    DWORD flags = 0;

    if (startSuspended) {
        flags |= CREATE_SUSPENDED;
    }

    result.handle = OwningHandle(CreateRemoteThread(
        process.raw(),
        &sa,
        0,
        func,
        param,
        flags,
        &result.tid
    )).validate();

    return result;
}

}  // namespace abel
