#include <abel/Pipe.hpp>

#include <atomic>
#include <cstdio>

#include <abel/Error.hpp>

namespace abel {

Pipe Pipe::create(bool inheritHandles, DWORD bufSize) {
    Pipe result{};

    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = inheritHandles,
    };

    bool success = CreatePipe(
        result.read.raw_ptr(),
        result.write.raw_ptr(),
        &sa,
        bufSize
    );

    if (!success) {
        fail("Failed to create pipe");
    }

    result.read.validate();
    result.write.validate();

    return result;
}

Pipe Pipe::create_async(bool inheritHandles, DWORD bufSize) {
    static std::atomic<unsigned> pipe_id{0};

    char name[MAX_PATH] = {};

    sprintf_s(
        name,
        sizeof(name),
        "\\\\.\\Pipe\\RemoteCMD.%08x.%08x",
        GetCurrentProcessId(),
        pipe_id.fetch_add(1)
    );

    Pipe result{};

    SECURITY_ATTRIBUTES sa{
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = inheritHandles,
    };

    result.read = OwningHandle(CreateNamedPipeA(
        name,
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_WAIT,
        1,
        bufSize,
        bufSize,
        120 * 1000,
        &sa
    )).validate();

    result.write = OwningHandle(CreateFileA(
        name,
        GENERIC_WRITE,
        0,
        &sa,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL
    )).validate();

    return result;
}

}  // namespace abel
