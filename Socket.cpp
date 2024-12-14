#include <abel/Socket.hpp>

#include <memory>

#include <abel/Concurrency.hpp>

namespace abel {

OwningSocket Socket::create() {
    // TODO: Change if I ever want to inherit socket handles. For now it would only serve to leak the bound port
    return OwningSocket(
        WSASocketA(
            AF_INET,
            SOCK_STREAM,
            0,
            nullptr,
            0,
            WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT
        )
    ).validate();
}

OwningSocket Socket::connect(std::string host, uint16_t port) {
    OwningSocket result = Socket::create();

    timeval timeout{.tv_sec = 15, .tv_usec = 0};
    bool success = WSAConnectByNameA(result.raw(), host.c_str(), std::to_string(port).c_str(), nullptr, nullptr, nullptr, nullptr, &timeout, nullptr);
    if (!success) {
        fail_ws("Failed to connect to socket");
    }

    return result;
}

OwningSocket Socket::listen(uint16_t port) {
    OwningSocket result = Socket::create();

    sockaddr_in addr{
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = INADDR_ANY,
    };

    int status = ::bind(result.raw(), (sockaddr *)&addr, sizeof(addr));
    if (status == SOCKET_ERROR) {
        fail_ws("Failed to bind socket");
    }

    status = ::listen(result.raw(), SOMAXCONN);
    if (status == SOCKET_ERROR) {
        fail_ws("Failed to listen on socket");
    }

    return result;
}

OwningSocket Socket::accept() {
    return OwningSocket(::accept(raw(), nullptr, nullptr)).validate();
}

eof<size_t> Socket::read_into(std::span<unsigned char> data) {
    int read = ::recv(raw(), (char *)data.data(), (int)data.size(), 0);
    if (read == SOCKET_ERROR) {
        fail_ws("Failed to read from socket");
    }

    return eof((size_t)read, read == 0);
}

eof<size_t> Socket::write_from(std::span<const unsigned char> data) {
    int written = ::send(raw(), (const char *)data.data(), (int)data.size(), 0);
    if (written == SOCKET_ERROR) {
        fail_ws("Failed to write to socket");
    }

    return eof((size_t)written, written == 0);
}

//void Socket::cancel_async();

// WinAPI promises that WSAOVERLAPPED is compatible with OVERLAPPED,
// but this verifies this assumption
static_assert(sizeof(WSAOVERLAPPED) == sizeof(OVERLAPPED));

struct _impl_WSAAsyncData {
    WSABUF wsabuf;
    DWORD flags;

    _impl_WSAAsyncData(std::span<unsigned char> data) :
        wsabuf{.len = (ULONG)data.size(), .buf = (char *)data.data()},
        flags{0} {
    }
};

AIO<eof<size_t>> Socket::read_async_into(std::span<unsigned char> data) {
    auto &env = *co_await current_env{};
    WSAOVERLAPPED *overlapped = (WSAOVERLAPPED *)env.overlapped();

    auto wsadata = std::make_unique<_impl_WSAAsyncData>(data);

    int status = WSARecv(
        raw(),
        &wsadata->wsabuf,
        1,
        nullptr,
        &wsadata->flags,
        overlapped,
        nullptr
    );

    if (status == SOCKET_ERROR) {
        switch (WSAGetLastError()) {
        case WSA_IO_PENDING:
            break;
        case WSAECONNRESET:
        case WSAEDISCON:
            co_return eof((size_t)0, true);
        default:
            fail_ws("Failed to initiate asynchronous read from socket");
        }
    }

    co_await io_done_signaled{};

    DWORD transmitted = 0;
    DWORD flags = 0;
    bool success = WSAGetOverlappedResult(
        raw(),
        overlapped,
        &transmitted,
        false,
        &flags
    );

    if (!success) {
        switch (WSAGetLastError()) {
        case WSAECONNRESET:
        case WSAEDISCON:
            co_return eof((size_t)transmitted, true);
        default:
            fail_ws("Failed to get overlapped operation result");
        }
    }

    co_return eof((size_t)transmitted, transmitted == 0);
}

AIO<eof<size_t>> Socket::write_async_from(std::span<const unsigned char> data) {
    auto &env = *co_await current_env{};
    WSAOVERLAPPED *overlapped = (WSAOVERLAPPED *)env.overlapped();

    // Note: const violation is okay because WSASend mustn't write to this buffer
    auto wsadata = std::make_unique<_impl_WSAAsyncData>(std::span{const_cast<unsigned char *>(data.data()), data.size()});

    int status = WSASend(
        raw(),
        &wsadata->wsabuf,
        1,
        nullptr,
        wsadata->flags,
        overlapped,
        nullptr
    );

    if (status == SOCKET_ERROR) {
        switch (WSAGetLastError()) {
        case WSA_IO_PENDING:
            break;
        case WSAECONNRESET:
        case WSAEDISCON:
            co_return eof((size_t)0, true);
        default:
            fail_ws("Failed to initiate asynchronous write to socket");
        }
    }

    co_await io_done_signaled{};

    DWORD transmitted = 0;
    DWORD flags = 0;
    bool success = WSAGetOverlappedResult(
        raw(),
        overlapped,
        &transmitted,
        false,
        &flags
    );

    if (!success) {
        switch (WSAGetLastError()) {
        case WSAECONNRESET:
        case WSAEDISCON:
            co_return eof((size_t)transmitted, true);
        default:
            fail_ws("Failed to get overlapped operation result");
        }
    }

    co_return eof((size_t)transmitted, transmitted == 0);
}

void Socket::shutdown(int how) {
    int status = ::shutdown(raw(), how);
    if (status == SOCKET_ERROR) {
        fail_ws("Failed to shutdown socket");
    }
}

}  // namespace abel
