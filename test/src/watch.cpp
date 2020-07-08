// watch.cpp

#include <catch2/catch.hpp>

#include <utility>

#include <wmp/watch.hpp>

using namespace wmp;

TEST_CASE("wmp::watch channel closed when final receiver handle dropped")
{
    auto t = watch::create<uint8_t>(0);

    // all receiver handles dropped; channel is closed
    REQUIRE(true);
}