// watch1.cpp

#include <wmp/watch.hpp>

constexpr static auto const SUCCESS = 0x0;
constexpr static auto const FAILURE = 0x1;

struct some_data
{
    uint32_t x;
    uint32_t y;

    some_data(uint32_t const x_, uint32_t const y_)
        : x{x_}, y{y_} {}
};

auto main() -> int
{
    using namespace wmp;

    // create some object to monitor for changes in multithreaded environment
    auto obj = some_data{1, 2};

    //auto [tx, rx] = watch::create(obj);

    return SUCCESS;
}