#pragma once

#include <abel/Handle.hpp>

#include <Windows.h>
#include <utility>

namespace abel {

class Pipe {
public:
    OwningHandle read{};
    OwningHandle write{};

    constexpr Pipe() {
    }

    constexpr Pipe(Pipe &&other) noexcept = default;
    constexpr Pipe &operator=(Pipe &&other) noexcept = default;

    static Pipe create(bool inheritHandles = true, DWORD bufSize = 0);

    // Actually creates a named pipe secretly, because unnamed pipes, as it turns out, do not support overlapped IO.
    static Pipe create_async(bool inheritHandles = true, DWORD bufSize = 0);
};

}  // namespace abel
