// mpsc1.cpp

#include <cstdio>
#include <cassert>

#include <wmp/mpsc.hpp>

constexpr static auto const SUCCESS = 0x0;
constexpr static auto const FAILURE = 0x1;

auto main() -> int
{
    using namespace wmp;

    auto [tx, rx] = mpsc::create<uint8_t>(100);

    auto const value = uint8_t{42};

    auto const r = tx.send(value);
    assert(mpsc::send_result::success == r);

    printf("Sent value: %u\n", value);

    auto const v = rx.recv();
    assert(v.has_value());

    printf("Received value: %u\n", v.value());

    return SUCCESS;
}