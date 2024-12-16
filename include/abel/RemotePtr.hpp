#pragma once

#include <abel/Handle.hpp>
#include <abel/Error.hpp>

#include <Windows.h>
#include <utility>
#include <concepts>
#include <type_traits>

namespace abel {

namespace _impl {

template <typename T>
class NonVoidPtr {};

template <typename T>
requires (!std::is_void_v<T>)
class NonVoidPtr<T> {
public:
    T read(this const auto &self) {
        T result{};
        bool success = ReadProcessMemory(
            self.process,
            self.ptr,
            &result,
            sizeof(T),
            nullptr
        );
        if (!success) {
            fail("Failed to read process memory");
        }
        return result;
    }

    void write(this const auto &self, const T &value) {
        bool success = WriteProcessMemory(
            self.process,
            self.ptr,
            &value,
            sizeof(T),
            nullptr
        );
        if (!success) {
            fail("Failed to write process memory");
        }
    }

    template <typename Self>
    struct reference_type {
        const Self &ptr;

        reference_type(Self &ptr) :
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

    template <typename Self>
    reference_type<Self> operator*(this const Self &self) {
        return {self};
    }

    template <typename Self>
    reference_type<Self> operator->(this const Self &self) {
        return {self};
    }
};

}  // namespace _impl

template <typename T = void>
class RemotePtr : public _impl::NonVoidPtr<T> {
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

    T *raw() const noexcept {
        return ptr;
    }

    operator bool() const noexcept {
        return process && ptr;
    }

    template <typename U>
    RemotePtr<U> cast() const {
        return {process, (U *)(ptr)};
    }

    template <typename U>
    operator RemotePtr<U>() const {
        return cast<U>();
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
};

}  // namespace abel
