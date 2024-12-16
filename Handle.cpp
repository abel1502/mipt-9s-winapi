#include <abel/Handle.hpp>

#include <abel/Error.hpp>
#include <abel/Concurrency.hpp>
#include <abel/RemotePtr.hpp>

namespace abel {

void Handle::close() {
    bool success = CloseHandle(value);
    value = NULL;

    if (!success) {
        fail("Failed to close handle");
    }
}

OwningHandle Handle::clone() const {
    OwningHandle result{};

    bool success = DuplicateHandle(
        GetCurrentProcess(),
        value,
        GetCurrentProcess(),
        result.raw_ptr(),
        0,
        false,
        DUPLICATE_SAME_ACCESS
    );

    if (!success) {
        fail("Failed to duplicate handle");
    }

    return result;
}

#pragma region IO
eof<size_t> Handle::read_into(std::span<unsigned char> data) {
    DWORD read = 0;
    bool success = ReadFile(raw(), data.data(), (DWORD)data.size(), &read, nullptr);
    if (!success) {
        fail("Failed to read from handle");
    }

    return eof((size_t)read, read == 0);
}

eof<size_t> Handle::write_from(std::span<const unsigned char> data) {
    DWORD written = 0;
    bool success = WriteFile(raw(), data.data(), (DWORD)data.size(), &written, nullptr);
    if (!success) {
        fail("Failed to write to handle");
    }
    // MSDN seems to imply a successful WriteFile call always writes the entire buffer
    assert(written == data.size());

    return eof((size_t)written, written == 0);
}

void Handle::cancel_async() {
    CancelIo(raw());
}

AIO<eof<size_t>> Handle::read_async_into(std::span<unsigned char> data) {
    auto &env = *co_await current_env{};
    OVERLAPPED *overlapped = env.overlapped();

    bool success = ReadFile(
        raw(),
        data.data(),
        (DWORD)data.size(),
        nullptr,
        overlapped
    );

    if (!success && GetLastError() != ERROR_IO_PENDING) {
        fail("Failed to initiate asynchronous read from handle");
    }

    co_await io_done_signaled{};

    DWORD transmitted = 0;
    success = GetOverlappedResultEx(
        raw(),
        overlapped,
        &transmitted,
        0,
        false
    );

    if (!success) {
        fail("Failed to get overlapped operation result");
    }

    // TODO: Perhaps a GetLastError check is necessary instead?
    co_return eof((size_t)transmitted, transmitted == 0);
}

AIO<eof<size_t>> Handle::write_async_from(std::span<const unsigned char> data) {
    auto &env = *co_await current_env{};
    OVERLAPPED *overlapped = env.overlapped();

    bool success = WriteFile(
        raw(),
        data.data(),
        (DWORD)data.size(),
        nullptr,
        overlapped
    );

    if (!success && GetLastError() != ERROR_IO_PENDING) {
        fail("Failed to initiate asynchronous write to handle");
    }

    co_await io_done_signaled{};

    DWORD transmitted = 0;
    success = GetOverlappedResultEx(
        raw(),
        overlapped,
        &transmitted,
        0,
        false
    );

    if (!success) {
        fail("Failed to get overlapped operation result");
    }

    // TODO: Perhaps a GetLastError check is necessary instead?
    co_return eof((size_t)transmitted, transmitted == 0);
}
#pragma endregion IO

#pragma region Synchronization
OwningHandle Handle::create_event(bool manualReset, bool initialState, bool inheritHandle) {
    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = inheritHandle,
    };

    return OwningHandle(CreateEvent(&sa, manualReset, initialState, nullptr)).validate();
}

OwningHandle Handle::create_mutex(bool initialOwner, bool inheritHandle) {
    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = inheritHandle,
    };

    return OwningHandle(CreateMutex(&sa, initialOwner, nullptr)).validate();
}

void Handle::signal() {
    bool success = SetEvent(raw());
    if (!success) {
        fail("Failed to signal event");
    }
}

void Handle::reset() {
    bool success = ResetEvent(raw());
    if (!success) {
        fail("Failed to reset event");
    }
}

bool Handle::is_signaled() const {
    return wait_timeout(0);
}

void Handle::wait() const {
    wait_timeout(INFINITE);
}

bool Handle::wait_timeout(DWORD miliseconds) const {
    DWORD result = WaitForSingleObject(raw(), miliseconds);
    switch (result) {
    case WAIT_OBJECT_0:
        return true;
    case WAIT_TIMEOUT:
        return false;
    case WAIT_FAILED:
        fail("Failed to wait on handle");
    case WAIT_ABANDONED:
        fail("Wait abandoned");
    default:
        fail("Unknown wait result");
    }
}

size_t Handle::wait_multiple(std::span<Handle> handles, bool all, DWORD miliseconds) {
    /*std::unique_ptr<HANDLE[]> handlesArr = std::make_unique<HANDLE[]>(handles.size());
    for (size_t i = 0; i < handles.size(); ++i) {
        handlesArr[i] = handles[i].raw();
    }*/

    // Note: Relies on handles being transparent wrappers over HANDLEs
    static_assert(sizeof(Handle) == sizeof(HANDLE));
    DWORD result = WaitForMultipleObjects((DWORD)handles.size(), (HANDLE *)handles.data(), all, miliseconds);

    if (WAIT_OBJECT_0 <= result && result < WAIT_OBJECT_0 + handles.size()) {
        return result - WAIT_OBJECT_0;
    }

    if (WAIT_ABANDONED_0 <= result && result < WAIT_ABANDONED_0 + handles.size()) {
        fail("Wait abandoned");
    }

    switch (result) {
    case WAIT_TIMEOUT:
        return (size_t)-1;
    case WAIT_FAILED:
        fail("Failed to wait on handles");
    case WAIT_ABANDONED:
        fail("Wait abandoned");
    default:
        fail("Unknown wait result");
    }
}
#pragma endregion Synchronization

#pragma region Thread
void Handle::suspend_thread() const {
    DWORD result = SuspendThread(raw());

    if (result == -1) {
        fail("Failed to suspend thread");
    }
}

void Handle::resume_thread() const {
    DWORD result = ResumeThread(raw());

    if (result == -1) {
        fail("Failed to resume thread");
    }
}

void Handle::terminate_thread(DWORD exit_code) {
    bool success = TerminateThread(raw(), exit_code);

    if (!success) {
        fail("Failed to terminate thread");
    }
}

void Handle::terminate_process(DWORD exit_code) {
    bool success = TerminateProcess(raw(), exit_code);

    if (!success) {
        fail("Failed to terminate process");
    }
}

DWORD Handle::get_exit_code_thread() const {
    DWORD result = 0;
    bool success = GetExitCodeThread(raw(), &result);

    if (!success) {
        fail("Failed to get thread exit code");
    }

    return result;
}

DWORD Handle::get_exit_code_process() const {
    DWORD result = 0;
    bool success = GetExitCodeProcess(raw(), &result);

    if (!success) {
        fail("Failed to get process exit code");
    }

    return result;
}

OwningHandle Handle::open_process(DWORD pid, DWORD desiredAccess, bool inheritHandle) {
    return OwningHandle(OpenProcess(desiredAccess, inheritHandle, pid)).validate();
}

RemotePtr<void> Handle::virtual_alloc(size_t size, DWORD allocationType, DWORD protect, void *address) {
    void *result = VirtualAllocEx(raw(), address, size, allocationType, protect);
    if (!result) {
        fail("Failed to allocate virtual memory");
    }
    return {*this, result};
}
#pragma endregion Thread

#pragma region Console
ConsoleEventPeek Handle::peek_console_input() {
    return ConsoleEventPeek(*this);
}

INPUT_RECORD Handle::read_console_input() {
    INPUT_RECORD result{};

    DWORD read = 0;
    bool success = ReadConsoleInputA(
        raw(),
        &result,
        1,
        &read
    );

    if (!success) {
        fail("Failed to read console input");
    }

    // WinAPI guarantees it's >0, and sucess means it has to be 1
    assert(read == 1);

    return result;
}

size_t Handle::console_input_queue_size() const {
    DWORD result = 0;
    bool success = GetNumberOfConsoleInputEvents(raw(), &result);

    if (!success) {
        fail("Failed to get console input queue size");
    }

    return result;
}

ConsoleAsyncIO Handle::console_async_io() {
    return ConsoleAsyncIO{*this};
}

AIO<eof<size_t>> ConsoleAsyncIO::read_async_into(std::span<unsigned char> data) {
    // printf("!!! console %p: reading...\n", handle.raw());

    co_await abel::event_signaled{handle};

    size_t read = 0;
    bool any_text = false;

    size_t queue_size = handle.console_input_queue_size();

    for (size_t i = 0; i < queue_size; ++i) {
        auto input = handle.peek_console_input();
        auto event = input.event();
        if (event.EventType != KEY_EVENT) {
            continue;
        }

        auto &key_event = event.Event.KeyEvent;
        if (!key_event.bKeyDown) {
            continue;
        }

        /*any_text = true;
        input.reject();
        break;*/

        char chr = key_event.uChar.AsciiChar;
        if (chr == 0) {
            // Things like shift, ctrl, etc.
            continue;
        }
        if (chr == '\r') {
            chr = '\n';
        }

        // If the event doesn't fit in the buffer's remainder, don't consume it either
        unsigned repeats = key_event.wRepeatCount;
        if (repeats > data.size()) {
            input.reject();
            break;
        }

        read += repeats;
        std::fill_n(data.begin(), repeats, chr);
        data = data.subspan(repeats);
    }

    /*DWORD read = 0;
    bool success = ReadConsoleA(handle.raw(), data.data(), (DWORD)data.size(), &read, nullptr);
    if (!success) {
        fail("Failed to read console input");
    }*/

    // Without this, input echo is delayed due to cmd.exe's echo cannot be forced immediately
    // With this, echo is doubled because cmd.exe's echo cannot be suppressed either...?
    // Wait, but it can. We just gotta pass /q... Alternatively, perhaps double echo is better, since
    // it handles backspace & stuff correctly
    WriteConsoleA(Handle::get_stdout().raw(), data.data() - read, (DWORD)read, nullptr, nullptr);

    // TODO: Detect eof from ctrl-something?
    // Note: read == 0 does NOT mean eof here, we could've just got exclusively mouse & etc. events
    co_return eof(read, false);
}

ConsoleEventPeek::ConsoleEventPeek(Handle handle) :
    handle{handle} {

    DWORD read = 0;
    bool success = PeekConsoleInputA(
        handle.raw(),
        &event_,
        1,
        &read
    );

    if (!success) {
        fail("Failed to peek console input");
    }

    // WinAPI guarantees it's >0, and sucess means it has to be 1
    assert(read == 1);
}

ConsoleEventPeek::~ConsoleEventPeek() {
    if (handle) {
        handle.read_console_input();
    }
}

// Note: actually sync under the hood; is needed to fake async transfers into console output
AIO<eof<size_t>> ConsoleAsyncIO::write_async_from(std::span<const unsigned char> data) {
    // printf("!!! console %p: writing...\n", handle.raw());

    co_return handle.write_from(data);
}

DWORD Handle::get_console_mode() const {
    DWORD result{};
    bool success = GetConsoleMode(raw(), &result);

    if (!success) {
        fail("Failed to get console mode");
    }

    return result;
}

void Handle::set_console_mode(DWORD mode) {
    bool success = SetConsoleMode(raw(), mode);

    if (!success) {
        fail("Failed to set console mode");
    }
}
#pragma endregion Console

}  // namespace abel
