#pragma once

#include <abel/Handle.hpp>

#include <Windows.h>
#include <utility>
#include <string>
#include <string_view>
#include <functional>

namespace abel {

class Process {
protected:
    Process() {
    }

public:
    OwningHandle process{};
    DWORD pid{};
    OwningHandle thread{};
    DWORD tid{};

    constexpr Process(Process &&other) noexcept = default;
    constexpr Process &operator=(Process &&other) noexcept = default;

    static Process create(
        const std::string &executable,
        const std::string &arguments = "",
        const std::string &workingDirectory = "",
        bool inheritHandles = false,
        DWORD creationFlags = 0,
        DWORD startupFlags = 0,
        Handle stdInput = nullptr,
        Handle stdOutput = nullptr,
        Handle stdError = nullptr,
        std::function<void(STARTUPINFOA &)> extraParams = nullptr
    );
};

}  // namespace abel
