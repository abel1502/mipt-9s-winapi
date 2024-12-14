#pragma once

#include <abel/Handle.hpp>
#include <abel/Error.hpp>

#include <Windows.h>
#include <utility>
#include <concepts>
#include <type_traits>

namespace abel {

template <typename T = void>
class RemotePtr {
protected:
    Handle process{};
    T *value = nullptr;

public:
    RemotePtr() noexcept = default;
    RemotePtr(nullptr_t) noexcept :
        RemotePtr() {
    }
    RemotePtr(Handle process, T *value) noexcept :
        process(process), ptr(value) {
    }

    RemotePtr(const RemotePtr &other) noexcept = default;
    RemotePtr &operator=(const RemotePtr &other) noexcept = default;

    RemotePtr(RemotePtr &&other) noexcept {
        std::swap(process, other.process);
        std::swap(ptr, other.ptr);
    }
    RemotePtr &operator=(RemotePtr &&other) noexcept {
        std::swap(process, other.process);
        std::swap(ptr, other.ptr);
        return *this;
    }

    operator bool() const noexcept {
        return process && value;
    }

    T read() requires !std::is_void<T> const {
    }

    void write(T value) requires !std::is_void<T> const {
    }

    template <typename U>
    RemotePtr<U> cast() const {
        return {process, (U *)(value)};
    }

    // TODO: Pointer arithmetic: add/sub size_t, sub RemotePtr<T>, custom case for void
    // TODO: Convenience wrappers: dereferencing proxy, cast operators
};

}  // namespace abel
