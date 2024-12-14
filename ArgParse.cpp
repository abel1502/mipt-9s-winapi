#include <abel/ArgParse.hpp>

namespace abel {

void ArgParser::parse(int argc, const char **argv) {
    args.assign(argv + 1, argv + argc);
    cur_pos = 0;

    // TODO: Try-catch for better error reporting?

    while (cur_pos < args.size()) {
        const auto &arg = lookup_known_arg(next_arg());
        arg.handler(*this);
    }
}

const ArgParser::arg &ArgParser::lookup_known_arg(std::string_view token) const {
    if (token.size() < 0) {
        fail("Empty argument");
    }
    if (!token.starts_with("-")) {
        fail("Positional arguments not supported");
    }

    if (!token.starts_with("--")) {
        if (token.size() != 2) {
            fail("Invalid shorthand argument");
        }

        for (const auto &arg : known_args) {
            if (arg.shorthand == token[1]) {
                return arg;
            }
        }

        fail("Unknown argument");
    }

    token.remove_prefix(2);

    for (const auto &arg : known_args) {
        if (arg.name == token) {
            return arg;
        }
    }

    fail("Unknown argument");
}

}  // namespace abel
