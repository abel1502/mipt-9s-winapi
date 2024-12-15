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
    T *ptr = nullptr;

public:
    RemotePtr() noexcept = default;
    RemotePtr(nullptr_t) noexcept :
        RemotePtr() {
    }
    RemotePtr(Handle process, T *ptr) noexcept :
        process(process), ptr(ptr) {
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
        return process && ptr;
    }

    T read() /*requires !std::is_void<T>*/ const {
        T result{};
        bool success = ReadProcessMemory(
            process,
            ptr,
            &result,
            sizeof(T),
            nullptr
        );
        if (!success) {
            fail("Failed to read process memory");
        }
        return result;
    }

    void write(const T &value) /*requires !std::is_void<T>*/ const {
        bool success = WriteProcessMemory(
            process,
            ptr,
            &value,
            sizeof(T),
            nullptr
        );
        if (!success) {
            fail("Failed to write process memory");
        }
        return result;
    }

    template <typename U>
    RemotePtr<U> cast() const {
        return {process, (U *)(ptr)};
    }

    RemotePtr<T> operator+(size_t offset) const {
        return {process, ptr + offset};
    }

    RemotePtr<T> operator-(size_t offset) const {
        return {process, ptr - offset};
    }

    RemotePtr<T> &operator+=(size_t offset) {
        ptr += offset;
        return *this;
    }

    RemotePtr<T> &operator-=(size_t offset) {
        ptr -= offset;
        return *this;
    }

    RemotePtr<T> &operator++() {
        ++ptr;
        return *this;
    }

    RemotePtr<T> &operator--() {
        --ptr;
        return *this;
    }

    struct reference_type {
        const RemotePtr<T> &ptr;

        reference_type(RemotePtr<T> &ptr) :
            ptr(ptr) {
        }

        reference_type(const reference_type &other) = default;
        reference_type &operator=(const reference_type &other) = default;

        operator T() {
            return ptr.read();
        }

        T operator=(const T &value) {
            ptr.write(value);
            return value;
        }
    };

    reference_type operator*() const {
        return {*this};
    }

    reference_type operator->() const {
        return {*this};
    }
};

}  // namespace abel
