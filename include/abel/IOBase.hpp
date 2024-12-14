#pragma once

#include <abel/Error.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <memory>
#include <span>
#include <vector>
#include <cassert>

namespace abel {

template <typename T>
class AIO;

class IOBase;

struct unit {};

template <typename T>
struct eof {
    [[msvc::no_unique_address]] T value;
    bool is_eof{false};

    template <typename F>
    constexpr eof<std::invoke_result_t<F, T>> convert(F &&f) const {
        return {std::invoke(std::forward<F>(f), value), is_eof};
    }

    constexpr eof<unit> discard_value() const {
        return {unit{}, is_eof};
    }
};

template <typename T>
concept sync_readable = std::derived_from<T, IOBase> && requires(T t, std::span<unsigned char> buf) {
    { t.read_into(buf) } -> std::same_as<eof<size_t>>;
};

template <typename T>
concept sync_writable = std::derived_from<T, IOBase> && requires(T t, std::span<const unsigned char> buf) {
    { t.write_from(buf) } -> std::same_as<eof<size_t>>;
};

template <typename T>
concept sync_io = sync_readable<T> && sync_writable<T>;

template <typename T>
concept async_readable = std::derived_from<T, IOBase> && requires(T t, std::span<unsigned char> buf) {
    { t.read_async_into(buf) } -> std::same_as<AIO<eof<size_t>>>;
};

template <typename T>
concept async_writable = std::derived_from<T, IOBase> && requires(T t, std::span<const unsigned char> buf) {
    { t.write_async_from(buf) } -> std::same_as<AIO<eof<size_t>>>;
};

template <typename T>
concept async_io = async_readable<T> && async_writable<T>;

class IOBase {
public:
    template <typename Self>
    requires sync_readable<Self>
    eof<unit> read_full_into(this Self &self, std::span<unsigned char> buf) {
        eof<size_t> result{0, false};
        while (buf.size() > 0 && !result.is_eof) {
            result = self.read_into(buf);
            buf = buf.subspan(result.value);
        }

        if (result.is_eof && buf.size() > 0) {
            fail("End of stream reached prematurely");
        }

        return result.discard_value();
    }

    template <typename Self>
    requires sync_writable<Self>
    eof<unit> write_full_from(this Self &self, std::span<const unsigned char> buf) {
        eof<size_t> result{0, false};
        while (buf.size() > 0 && !result.is_eof) {
            result = self.write_from(buf);
            buf = buf.subspan(result.value);
        }

        if (result.is_eof && buf.size() > 0) {
            fail("End of stream reached prematurely");
        }

        return result.discard_value();
    }

    template <typename Self>
    requires sync_readable<Self>
    eof<std::vector<unsigned char>> read(this Self &self, size_t size, bool exact = false) {
        eof<std::vector<unsigned char>> result(std::vector<unsigned char>(size), false);

        if (exact) {
            result.is_eof = self.read_full_into(result.value).is_eof;
        } else {
            eof<size_t> read = self.read_into(result.value);
            result.value.resize(read.value);
            result.is_eof = read.is_eof;
        }

        return result;
    }

    template <typename Self>
    requires async_readable<Self>
    AIO<eof<unit>> read_async_full_into(this Self &self, std::span<unsigned char> buf) {
        eof<size_t> result{0, false};
        while (buf.size() > 0 && !result.is_eof) {
            result = co_await self.read_async_into(buf);
            buf = buf.subspan(result.value);
        }

        if (result.is_eof && buf.size() > 0) {
            fail("End of stream reached prematurely");
        }

        co_return result.discard_value();
    }

    template <typename Self>
    requires async_writable<Self>
    AIO<eof<unit>> write_async_full_from(this Self &self, std::span<const unsigned char> buf) {
        eof<size_t> result{0, false};
        while (buf.size() > 0 && !result.is_eof) {
            result = co_await self.write_async_from(buf);
            buf = buf.subspan(result.value);
        }

        if (result.is_eof && buf.size() > 0) {
            fail("End of stream reached prematurely");
        }

        co_return result.discard_value();
    }

    template <typename Self>
    requires async_readable<Self>
    AIO<eof<std::vector<unsigned char>>> read_async(this Self &self, size_t size, bool exact = false) {
        eof<std::vector<unsigned char>> result(std::vector(size), false);

        if (exact) {
            result.is_eof = co_await self.read_async_full_into(result).is_eof;
        } else {
            eof<size_t> read = co_await self.read_async_into(result.value);
            result.value.resize(read.value);
            result.is_eof = read.is_eof;
        }

        co_return result;
    }
};

template <async_readable S, async_writable D>
AIO<void> async_transfer(S src, D dst, size_t buf_size = 4096) {
    std::unique_ptr<unsigned char[]> buf = std::make_unique<unsigned char[]>(buf_size);
    while (true) {
        //printf("!!! async_transfer %p->%p: reading...\n", &src, &dst);
        auto read_result = co_await src.read_async_into({buf.get(), buf_size});
        if (read_result.is_eof) {
            break;
        }
        //printf("!!! async_transfer %p->%p: writing \"%.*s\"...\n", &src, &dst, (int)read_result.value, buf.get());
        auto write_result = co_await dst.write_async_full_from({buf.get(), read_result.value});
        if (write_result.is_eof) {
            break;
        }
    }
}

}  // namespace abel
