#pragma once

#include <abel/Error.hpp>
#include <abel/Handle.hpp>
#include <abel/IOBase.hpp>

#include <WinSock2.h>
#include <Windows.h>
#include <string>
#include <cstdint>
#include <utility>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

namespace abel {

class OwningSocket;

class Socket : public IOBase {
protected:
    SOCKET socket{INVALID_SOCKET};

    static OwningSocket create();

public:
    constexpr Socket() noexcept :
        socket(INVALID_SOCKET) {
    }

    constexpr Socket(SOCKET value) noexcept :
        socket(value) {
    }

    constexpr Socket(const Socket &other) noexcept = default;
    constexpr Socket &operator=(const Socket &other) noexcept = default;
    constexpr Socket(Socket &&other) noexcept = default;
    constexpr Socket &operator=(Socket &&other) noexcept = default;

    static OwningSocket connect(std::string host, uint16_t port);

    // TODO: Accept host?
    static OwningSocket listen(uint16_t port);

    constexpr operator bool() const noexcept {
        return socket != INVALID_SOCKET;
    }

    template <typename Self>
    constexpr Self &&validate(this Self &&self) {
        if (!self) {
            fail_ws("Socket is invalid");
        }
        return std::forward<Self>(self);
    }

    constexpr SOCKET raw() const {
        return socket;
    }

    OwningSocket accept();

#pragma region IO
    // Technically allowed by WinAPI, but may involve overhead delays depending on the implementation
    Handle io_handle() const noexcept {
        return Handle{(HANDLE)socket};
    }

    // Reads some data into the buffer. Returns the number of bytes read and eof status.
    eof<size_t> read_into(std::span<unsigned char> data);

    // Writes the contents. Returns the number of bytes written and eof status. All bytes must be written after a successful invocation.
    eof<size_t> write_from(std::span<const unsigned char> data);

    // Note: asynchronous IO requires the handle to have been opened with FILE_FLAG_OVERLAPPED

    // TODO: Is this even possible?
    // Cancels all pending async operations on this handle
    //void cancel_async();

    // Same as read_into, but returns an awaitable. Note: the buf must not be located in a coroutine stack.
    AIO<eof<size_t>> read_async_into(std::span<unsigned char> data);

    // Same as write_from, but returns an awaitable. Note: the buf must not be located in a coroutine stack.
    AIO<eof<size_t>> write_async_from(std::span<const unsigned char> data);
#pragma endregion IO

    void shutdown(int how = SD_BOTH);
};

class OwningSocket : public Socket {
public:
    constexpr OwningSocket() noexcept :
        Socket() {
    }

    constexpr OwningSocket(SOCKET value) noexcept :
        Socket(value) {
    }

    OwningSocket(const OwningSocket &other) = delete;
    OwningSocket &operator=(const OwningSocket &other) = delete;

    constexpr OwningSocket(OwningSocket &&other) noexcept :
        Socket(std::move(other)) {
        other.socket = INVALID_SOCKET;
    }

    constexpr OwningSocket &operator=(OwningSocket &&other) noexcept {
        std::swap(socket, other.socket);
        return *this;
    }

    ~OwningSocket() noexcept {
        if (socket != INVALID_SOCKET) {
            closesocket(socket);
            socket = INVALID_SOCKET;
        }
    }

    constexpr Socket borrow() const {
        return (Socket)*this;
    }
};

// An instance of this must be alive throughout the period sockets are intended to be used.
// This handles the WSAStartup and WSACleanup calls in a RAII-friendly way.
class SocketLibGuard {
protected:
    WSADATA wsadata{};

public:
    SocketLibGuard() {
        int result = WSAStartup(MAKEWORD(2, 2), &wsadata);
        if (result) {
            fail_ws("WSAStartup failed", result);
        }
        // TODO: Check version in wsadata
    }

    constexpr const WSADATA &info() const {
        return wsadata;
    }

    ~SocketLibGuard() {
        WSACleanup();
    }
};

}  // namespace abel
