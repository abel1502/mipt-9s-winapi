#pragma once

#include <abel/Error.hpp>
#include <abel/IOBase.hpp>

#include <Windows.h>
#include <utility>
#include <vector>
#include <span>
#include <cstdint>
#include <optional>
#include <concepts>
#include <memory>


namespace abel {

class OwningHandle;

template <typename T>
class AIO;

class ConsoleAsyncIO;
class ConsoleEventPeek;

// Handle is a non-owning wrapper around a WinAPI HANDLE
class Handle : public IOBase {
protected:
    HANDLE value;

public:
    constexpr Handle() noexcept :
        value(NULL) {
    }

    constexpr Handle(nullptr_t) noexcept :
        Handle() {
    }

    constexpr Handle(HANDLE value) noexcept :
        value(value) {
    }

    constexpr Handle(const Handle &other) noexcept = default;
    constexpr Handle &operator=(const Handle &other) noexcept = default;
    constexpr Handle(Handle &&other) noexcept = default;
    constexpr Handle &operator=(Handle &&other) noexcept = default;

    constexpr operator bool() const noexcept {
        return value != NULL;
    }

    template <typename Self>
    constexpr Self &&validate(this Self &&self) {
        if (self.value == NULL || self.value == INVALID_HANDLE_VALUE) {
            fail("Handle is invalid");
        }
        return std::forward<Self>(self);
    }

    constexpr HANDLE raw() const noexcept {
        return value;
    }

    template <typename Self>
    constexpr decltype(auto) raw_ptr(this Self &&self) noexcept {
        return &self.value;
    }

    OwningHandle clone() const;

    void close();

#pragma region IO
    // Reads some data into the buffer. Returns the number of bytes read and eof status.
    eof<size_t> read_into(std::span<unsigned char> data);

    // Writes the contents. Returns the number of bytes written and eof status. All bytes must be written after a successful invocation.
    eof<size_t> write_from(std::span<const unsigned char> data);

    // Note: asynchronous IO (below) requires the handle to have been opened with FILE_FLAG_OVERLAPPED

    // Cancels all pending async operations on this handle
    void cancel_async();

    // Same as read_into, but returns an awaitable. Note: the buf must not be located in a coroutine stack.
    AIO<eof<size_t>> read_async_into(std::span<unsigned char> data);

    // Same as write_from, but returns an awaitable. Note: the buf must not be located in a coroutine stack.
    AIO<eof<size_t>> write_async_from(std::span<const unsigned char> data);
#pragma endregion IO

#pragma region Synchronization
    static OwningHandle create_event(bool manualReset = false, bool initialState = false, bool inheritHandle = false);

    // TODO: CRITICAL_SECTION appears to be a lighter-weight single-process alternative
    static OwningHandle create_mutex(bool initialOwner = false, bool inheritHandle = false);

    // Sets an event
    void signal();

    // Resets an event
    void reset();

    // Tells if the handle is signaled without waiting
    bool is_signaled() const;

    // Blocks until the handle is signaled
    void wait() const;

    // Returns true if the wait succeeded, false on timeout
    bool wait_timeout(DWORD miliseconds) const;

    // Combines the functionality of wait_timeout, wait (timeout=INFINITE) and is_set (timeout=0)
    // for several handles at once. Returns -1U on timeout
    template <bool all = false, DWORD miliseconds = INFINITE, std::convertible_to<Handle>... T>
    static size_t wait_multiple(const T &... handles) {
        Handle arrHandles[sizeof...(T)] = {(Handle)handles...};
        return wait_multiple(arrHandles, all, miliseconds);
    }

    // Same as the template version, but takes a span instead
    static size_t wait_multiple(std::span<Handle> handles, bool all = false, DWORD miliseconds = INFINITE);
#pragma endregion Synchronization

#pragma region Thread
    void suspend_thread() const;

    void resume_thread() const;

    void terminate_thread(DWORD exit_code = (DWORD)-1);

    void terminate_process(DWORD exit_code = (DWORD)-1);

    DWORD get_exit_code_thread() const;

    DWORD get_exit_code_process() const;

    bool thread_running() const {
        return get_exit_code_thread() == STILL_ACTIVE;
    }

    bool process_running() const {
        return get_exit_code_process() == STILL_ACTIVE;
    }
#pragma endregion Thread

#pragma region Console
    static Handle get_stdin() {
        return Handle{GetStdHandle(STD_INPUT_HANDLE)}.validate();
    }

    static Handle get_stdout() {
        return Handle{GetStdHandle(STD_OUTPUT_HANDLE)}.validate();
    }

    static Handle get_stderr() {
        return Handle{GetStdHandle(STD_ERROR_HANDLE)}.validate();
    }

    ConsoleEventPeek peek_console_input();

    INPUT_RECORD read_console_input();

    size_t console_input_queue_size() const;

    ConsoleAsyncIO console_async_io();

    DWORD get_console_mode() const;

    void set_console_mode(DWORD mode);
#pragma endregion Console
};

// A handle that closes itself on destruction
class OwningHandle : public Handle {
public:
    constexpr OwningHandle() noexcept :
        Handle() {
    }

    constexpr OwningHandle(nullptr_t) noexcept :
        Handle(nullptr) {
    }

    constexpr OwningHandle(HANDLE value) noexcept :
        Handle(value) {
    }

    OwningHandle(const OwningHandle &other) = delete;

    OwningHandle &operator=(const OwningHandle &other) = delete;

    constexpr OwningHandle(OwningHandle &&other) noexcept :
        Handle(std::move(other)) {
        other.value = NULL;
    }

    constexpr OwningHandle &operator=(OwningHandle &&other) noexcept {
        std::swap(value, other.value);
        return *this;
    }

    ~OwningHandle() noexcept {
        if (value) {
            CloseHandle(value);
            value = NULL;
        }
    }

    constexpr Handle borrow() const {
        return (Handle)*this;
    }
};

class ConsoleAsyncIO : public IOBase {
protected:
    Handle handle;

public:
    ConsoleAsyncIO(Handle handle) : handle(handle) {}

    ConsoleAsyncIO(const ConsoleAsyncIO &) = default;
    ConsoleAsyncIO &operator=(const ConsoleAsyncIO &) = default;
    ConsoleAsyncIO(ConsoleAsyncIO &&) = default;
    ConsoleAsyncIO &operator=(ConsoleAsyncIO &&) = default;

    AIO<eof<size_t>> read_async_into(std::span<unsigned char> data);
    AIO<eof<size_t>> write_async_from(std::span<const unsigned char> data);
};

class ConsoleEventPeek {
protected:
    Handle handle;
    INPUT_RECORD event_{};

public:
    ConsoleEventPeek(Handle handle);

    ConsoleEventPeek(const ConsoleEventPeek &) = delete;
    ConsoleEventPeek &operator=(const ConsoleEventPeek &) = delete;

    constexpr ConsoleEventPeek(ConsoleEventPeek &&other) noexcept :
        handle(std::move(other.handle)) {
        other.handle = nullptr;
    }

    constexpr ConsoleEventPeek &operator=(ConsoleEventPeek &&other) noexcept {
        std::swap(handle, other.handle);
        return *this;
    }

    ~ConsoleEventPeek();

    template <typename Self>
    constexpr auto &event(this Self &self) {
        return self.event_;
    }

    constexpr void reject() {
        handle = nullptr;
    }
};

}  // namespace abel
