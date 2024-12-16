#pragma once

#include <abel/Handle.hpp>
#include <abel/Owning.hpp>

#include <Windows.h>
#include <utility>
#include <concepts>
#include <functional>
#include <memory>

namespace abel {

class Thread {
protected:
    Thread() {
    }

public:
    OwningHandle handle{};
    DWORD tid{};

    constexpr Thread(Thread &&other) noexcept = default;
    constexpr Thread &operator=(Thread &&other) noexcept = default;

    static Thread create(
        LPTHREAD_START_ROUTINE func,
        void *param = nullptr,
        bool inheritHandles = false,
        bool startSuspended = false
    );

    template <typename T, void (T::*func)()>
    static Thread create(
        T *obj,
        bool inheritHandles = false,
        bool startSuspended = false
    ) {
        return create(
            [](void *arg) -> DWORD {
                (((T *)arg)->*func)();
                return 0;
            },
            obj,
            inheritHandles,
            startSuspended
        );
    }

    template <std::invocable<> F>
        requires std::same_as<std::invoke_result_t<F>, DWORD>
    [[nodiscard]] static Owning<Thread, std::unique_ptr<F>> create(
        F &&func,
        bool inheritHandles = false,
        bool startSuspended = false
    ) {
        std::unique_ptr<F> funcPtr = std::make_unique(std::forward<F>(func));
        return Owning(
            create(
                [](void *arg) -> DWORD { return std::invoke(*reinterpret_cast<F *>(arg)); },
                funcPtr.get(),
                inheritHandles,
                startSuspended
            ),
            std::move(funcPtr)
        );
    }

    static Thread create_remote(
        Handle process,
        LPTHREAD_START_ROUTINE func,
        void *param = nullptr,
        bool inheritHandles = false,
        bool startSuspended = false
    );
};

}  // namespace abel
