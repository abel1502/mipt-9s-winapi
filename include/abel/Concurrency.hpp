#pragma once

#include <abel/Handle.hpp>
#include <abel/Error.hpp>

#include <Windows.h>
#include <utility>
#include <coroutine>
#include <vector>
#include <exception>
#include <expected>
#include <concepts>
#include <cassert>
#include <memory>

namespace abel {

#pragma region impl
template <typename T>
struct _impl_promise_return {
    std::expected<T, std::exception_ptr> result = std::unexpected{nullptr};

    void return_value(T value) {
        result = std::move(value);
    }

    void unhandled_exception() {
        result = std::unexpected{std::current_exception()};
    }

    T get_result() {
        if (result.has_value()) {
            return std::move(result).value();
        }
        std::rethrow_exception(std::move(result).error());
    }
};

template <>
struct _impl_promise_return<void> {
    std::exception_ptr result = nullptr;

    void return_void() {
        result = nullptr;
    }

    void unhandled_exception() {
        result = std::current_exception();
    }

    void get_result() {
        if (result) {
            std::rethrow_exception(result);
        }
    }
};

template <typename T, std::same_as<T>... U>
std::vector<T> _impl_make_vector(U &&...values) {
    std::vector<T> result{};
    result.reserve(sizeof...(values));
    (result.emplace_back(std::forward<U>(values)), ...);
    return result;
}
#pragma endregion impl

//struct current_coro {
//};

struct current_env {
};

struct io_done_signaled {
};

// Note: do not use auto-reset events! Non-event awaitables are fine.
struct event_signaled {
    Handle event;
};

class AIOEnv {
protected:
    OwningHandle io_done_ = Handle::create_event(true, true);  // TODO: Different flags?
    OVERLAPPED overlapped_{.hEvent = io_done_.raw()};
    Handle non_io_event_ = nullptr;
    std::coroutine_handle<> current_{nullptr};

public:
    AIOEnv() = default;

    template <typename T>
    void attach(AIO<T> &aio) {
        // printf("!!! Root %p: env=%p\n", aio.coro.address(), this);
        aio.coro.promise().env = this;
        current_ = aio.coro;
    }

    AIOEnv(const AIOEnv &other) = delete;
    AIOEnv &operator=(const AIOEnv &other) = delete;
    AIOEnv(AIOEnv &&other) = delete;
    AIOEnv &operator=(AIOEnv &&other) = delete;

    Handle event_done() const noexcept {
        if (non_io_event_) {
            return non_io_event_;
        }
        return io_done_;
    }

    OVERLAPPED *overlapped() noexcept {
        return &overlapped_;
    }

    void set_non_io_event(Handle event) noexcept {
        non_io_event_ = event;
    }

    std::coroutine_handle<> current() const noexcept {
        return current_;
    }

    void update_current(std::coroutine_handle<> prev, std::coroutine_handle<> coro) noexcept;

    void step();
};

// AIO is a coroutine object for simple asynchronous IO on WinAPI handles.
// It is also used as an awaitable for async IO primitives.
template <typename T = void>
class [[nodiscard]] AIO {
public:
    struct promise_type : public _impl_promise_return<T> {
        AIOEnv *env;
        std::coroutine_handle<> parent = nullptr;

        AIO get_return_object() {
            return AIO{coroutine_ptr::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept {
            return {};
        }

        auto final_suspend() noexcept {
            struct Awaiter {
                bool await_ready() noexcept {
                    return false;
                };

                std::coroutine_handle<> await_suspend(coroutine_ptr self) noexcept {
                    self.promise().env->update_current(self, self.promise().parent);
                    return self.promise().parent ? self.promise().parent : std::noop_coroutine();
                }

                void await_resume() noexcept {
                }
            };

            return Awaiter{};
        }

        /*auto await_transform(current_coro) {
            struct Awaiter {
                coroutine_ptr current{};

                bool await_ready() noexcept {
                    return false;
                }

                bool await_suspend(coroutine_ptr coro) noexcept {
                    current = coro;
                    return false;
                }

                coroutine_ptr await_resume() noexcept {
                    return current;
                }
            };

            return Awaiter{};
        }*/

        auto await_transform(current_env) {
            struct Awaiter {
                AIOEnv *env{};

                bool await_ready() noexcept {
                    return false;
                }

                bool await_suspend(coroutine_ptr) noexcept {
                    return false;
                }

                AIOEnv *await_resume() noexcept {
                    return env;
                }
            };

            return Awaiter{env};
        }

        auto await_transform(io_done_signaled) {
            struct Awaiter {
                AIOEnv *env;

                bool await_ready() noexcept {
                    return false;
                }

                void await_suspend(coroutine_ptr coro) {
                    // Just to verify we are the current coroutine
                    env->update_current(coro, coro);
                }

                void await_resume() {
                }
            };

            return Awaiter{env};
        }

        auto await_transform(event_signaled event) {
            struct Awaiter {
                AIOEnv *env;
                Handle event;

                bool await_ready() noexcept {
                    return false;
                }

                void await_suspend(coroutine_ptr coro) {
                    // Just to verify we are the current coroutine
                    env->update_current(coro, coro);
                    env->set_non_io_event(event);
                }

                void await_resume() {
                }
            };

            return Awaiter{env, event.event};
        }

        decltype(auto) await_transform(auto &&x) {
            return std::forward<decltype(x)>(x);
        }
    };

    using coroutine_ptr = std::coroutine_handle<promise_type>;

protected:
    coroutine_ptr coro;

    friend AIOEnv;

public:
    explicit AIO(coroutine_ptr coro) :
        coro{coro} {
    }

    AIO(const AIO &other) = delete;
    AIO &operator=(const AIO &other) = delete;

    constexpr AIO(AIO &&other) :
        coro{std::move(other.coro)} {
        other.coro = nullptr;
    }

    constexpr AIO &operator=(AIO &&other) {
        std::swap(coro, other.coro);
    }

    ~AIO() {
        if (coro) {
            coro.destroy();
        }
        coro = nullptr;
    }

    void step() {
        // TODO: ?
        coro.resume();
    }

    //Handle io_done() const {
    //    return coro.promise().io_done;
    //}

    //OVERLAPPED *overlapped() const {
    //    return &coro.promise().overlapped;
    //}

    bool await_ready() noexcept {
        return coro.done();
    }

    template <typename U>
    void await_suspend(std::coroutine_handle<U> master) noexcept {
        // printf("!!! Child %p -> %p: env=%p\n", master.address(), coro.address(), master.promise().env);
        auto &self_promise = coro.promise();
        auto &master_promise = master.promise();
        self_promise.env = master_promise.env;
        self_promise.parent = master;
        self_promise.env->update_current(master, coro);

        coro.resume();

        // TODO: Don't return control to master until we've returned a value
    }

    T await_resume() {
        // TODO: ?

        return coro.promise().get_result();
    }
};

class ParallelAIOs {
protected:
    std::vector<AIO<void>> tasks;
    std::unique_ptr<AIOEnv[]> envs;
    std::unique_ptr<Handle[]> events;

public:
    template <std::same_as<AIO<void>> ... T>
    ParallelAIOs(T &&...tasks) :
        ParallelAIOs(_impl_make_vector<AIO<void>>(std::forward<T>(tasks)...)) {
    }

    ParallelAIOs(std::vector<AIO<void>> tasks);

    size_t size() const {
        return tasks.size();
    }

    template <typename Self>
    decltype(auto) until(this Self &&self, Handle event) {
        self.events[0] = event;
        return std::forward<Self>(self);
    }

    void wait_any(DWORD miliseconds = INFINITE);

    void step();

    bool done() const;

    void run();
};

}  // namespace abel
