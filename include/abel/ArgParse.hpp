#pragma once

#include <abel/Error.hpp>

#include <vector>
#include <string>
#include <string_view>
#include <utility>
#include <functional>
#include <cstdint>
#include <charconv>

namespace abel {

class ArgParser {
public:
    using handler_func_t = void(ArgParser &parser);

    using handler_t = std::function<handler_func_t>;

    struct arg {
        std::string_view name;
        handler_t handler;
        char shorthand = 0;

        arg(std::string_view name, handler_t handler, char shorthand = 0) :
            name{std::move(name)}, handler{std::move(handler)}, shorthand{shorthand} {
        }
    };

protected:
    std::vector<arg> known_args{};
    std::vector<std::string_view> args{};  // Note: contents are guaranteed to be null-terminated strings, as they originate from const char **argv
    size_t cur_pos = 0;

public:
    ArgParser() {
    }

    void parse(int argc, const char **argv);

    void add_arg(std::string_view name, handler_t handler, char shorthand = 0) noexcept {
        known_args.push_back(arg{std::move(name), std::move(handler), shorthand});
    }

    const std::vector<arg> &get_known_args() const {
        return known_args;
    }

    const arg &lookup_known_arg(std::string_view token) const;

    constexpr const std::vector<std::string_view> &get_args() const {
        return args;
    }

    constexpr size_t get_cur_pos() const {
        return cur_pos;
    }

    std::string_view next_arg() {
        check_arg();
        move();
        return peek_arg(-1);
    }

    void check_arg(int delta = 0) const {
        if (cur_pos < -delta || cur_pos + delta > args.size()) {
            fail("Missing arguments");
        }
    }

    constexpr std::string_view peek_arg(int delta = 0) const {
        return args.at(cur_pos + delta);
    }

    constexpr void move(int delta = 1) noexcept {
        cur_pos += delta;
    }

    template <std::constructible_from<const char *> T>
    static handler_t handler_store_str(T &destination) {
        return [&destination](ArgParser &parser) {
            destination = parser.next_arg().data();
        };
    }

    template <std::integral T>
    static handler_t handler_store_int(T &destination) {
        return [&destination](ArgParser &parser) {
            std::string_view arg = parser.next_arg();
            auto status = std::from_chars(arg.data(), arg.data() + arg.size(), destination);
            if (status.ec != std::errc{} || status.ptr != arg.data() + arg.size()) {
                fail("Invalid argument");
            }
        };
    }

    static handler_t handler_store_flag(bool &destination) {
        return [&destination](ArgParser &parser) {
            destination = true;
        };
    }

    static handler_t handler_help(std::string_view message) {
        return [message](ArgParser &) {
            printf("%.*s\n", (int)message.size(), message.data());
            exit(0);
        };
    }
};

}  // namespace abel
