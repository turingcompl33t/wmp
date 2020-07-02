// oneshot1.cpp
//
// Basic usage of wmp::oneshot in a single-threaded context.

#include <cstdio>
#include <cassert>

#include <wmp/oneshot.hpp>

constexpr static auto const SUCCESS = 0x0;
constexpr static auto const FAILURE = 0x1;

auto main() -> int
{
    using namespace wmp;

    auto [tx, rx] = oneshot::create<uint8_t>();

    // initially, asynchronous try_recv() should fail, value not present
    auto const v1 = rx.try_recv();
    assert(!v1.has_value());

    printf("Initial try_recv() returned empty std::optional\n");

    // sender sends a value over the channel, asynchronously
    auto const r = tx.send_async(42);
    assert(oneshot::send_result::success == r);

    printf("Sender async_send() succeeded\n");

    // now, an asynchronous try_recv() should succeed
    auto const v2 = rx.try_recv();
    assert(v2.has_value());

    printf("Now try_recv() returns std::optional with value: %u\n", v2.value());

    return SUCCESS;
}
