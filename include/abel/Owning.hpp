#pragma once

#include <utility>
#include <tuple>

namespace abel {

template <typename T, typename... Us>
class Owning : public T {
private:
    std::tuple<Us...> owned;

public:
    Owning(T object, Us... owned) :
        T(std::move(object)),
        owned{std::make_tuple(std::move(owned)...)} {
    }
};

}  // namespace abel
