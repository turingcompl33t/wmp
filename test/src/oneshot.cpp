// oneshot.cpp
//
// Unit tests for wmp::oneshot

#include <catch2/catch.hpp>

#include <wmp/oneshot.hpp>

using namespace wmp;

TEST_CASE("wmp::oneshot single-threaded async_send(), try_recv()")
{
    auto [tx, rx] = oneshot::create<uint8_t>();

    auto const r = tx.send_async(42);
    REQUIRE(oneshot::send_result::success == r);

    auto const v = rx.try_recv();
    REQUIRE(v.has_value());
    REQUIRE(v.value() == 42);
}

TEST_CASE("wmp::oneshot single-threaded sender explicit close(), receiver try_recv()")
{
    auto [tx, rx] = oneshot::create<uint8_t>();

    tx.close();

    auto const v = rx.try_recv();
    REQUIRE_FALSE(v.has_value());
}

TEST_CASE("wmp::oneshot single-threaded sender explicit close(), receiver recv()")
{
    auto [tx, rx] = oneshot::create<uint8_t>();

    tx.close();

    auto const v = rx.recv();
    REQUIRE_FALSE(v.has_value());
}

TEST_CASE("wmp::oneshot single-threaded receiver explicit close(), sender send_async()")
{
    auto [tx, rx] = oneshot::create<uint8_t>();

    rx.close();

    auto const r = tx.send_async(42);
    REQUIRE(oneshot::send_result::failure == r);
}

TEST_CASE("wmp::oneshot single-threaded receiver explicit close(), sender send_sync()")
{
    auto [tx, rx] = oneshot::create<uint8_t>();

    rx.close();

    auto const r = tx.send_sync(42);
    REQUIRE(oneshot::send_result::failure == r);
}