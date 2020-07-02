// mpsc.cpp
//
// Unit tests for wmp::mpsc

#include <catch2/catch.hpp>

#include <wmp/mpsc.hpp>

using namespace wmp;

TEST_CASE("wmp::mpsc basic non-blocking send and receive")
{
    auto [tx, rx] = mpsc::create<uint8_t>(10);

    auto const v1 = rx.try_recv();
    REQUIRE_FALSE(v1.has_value());

    auto const value = 42;

    auto const r = tx.try_send(value);
    REQUIRE(mpsc::send_result::success == r);

    auto const v2 = rx.try_recv();
    REQUIRE(v2.has_value());
    REQUIRE(v2.value() == value);
}

TEST_CASE("wmp::mpsc try_send() on full buffer")
{
    auto [tx, rx] = mpsc::create<uint8_t>(1);

    auto const value = 42;

    auto r1 = tx.try_send(value);
    REQUIRE(mpsc::send_result::success == r1);

    auto r2 = tx.try_send(value);
    REQUIRE(mpsc::send_result::failure == r2);
}

TEST_CASE("wmp::mpsc sender explicit clone()")
{
    auto [tx, rx] = mpsc::create<uint8_t>(10);
    auto tx2 = tx.clone();

    auto const value = 42;

    auto const r1 = tx.try_send(value);
    REQUIRE(mpsc::send_result::success == r1);

    auto const r2 = tx.try_send(value);
    REQUIRE(mpsc::send_result::success == r2);

    auto const v1 = rx.try_recv();
    REQUIRE(v1.has_value());
    REQUIRE(v1.value() == value);

    auto const v2 = rx.try_recv();
    REQUIRE(v2.has_value());
    REQUIRE(v2.value() == value);
}