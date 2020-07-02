// oneshot2.cpp
//
// Basic usage of wmp::oneshot channel to pass a single message between threads.

#include <vector>
#include <thread>
#include <cstdio>

#include <wmp/oneshot.hpp>

static constexpr auto const SUCCESS = 0x0;
static constexpr auto const FAILURE = 0x1;

auto main() -> int
{
    using namespace wmp;

    auto [tx, rx] = oneshot::create<uint8_t>();

    auto threads = std::vector<std::thread>{};
    threads.emplace_back([tx = std::move(tx)]() mutable -> void {
        auto const v = uint8_t{42};
        auto const r = tx.send_async(v);
        printf("Sent value: %u with status: %s\n", 
            v, 
            oneshot::send_result::success == r 
                ? "Success" 
                : "Failure");
    });
    threads.emplace_back([rx = std::move(rx)]() mutable -> void {
        auto result = rx.recv();
        if (result.has_value())
        {
            printf("Received value: %u\n", result.value());
        }
        else
        {
            printf("receiver.recv() failed\n");
        }
    });

    for (auto& t : threads)
    {
        t.join();
    }

    return SUCCESS;
}