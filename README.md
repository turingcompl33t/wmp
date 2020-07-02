## Windows Message Passing

A small, header-only library for asynchronous message-passing.

### Contents

- [oneshot](include/wmp/oneshot.hpp) - a single-use single-producer, single-consumer channel
- [mpsc](include/wmp/mpsc.hpp) - a multi-use multiple-producer, single-consumer channel
- [bus](include/wmp/bus.hpp) - a multi-use multiple-producer, multiple-consumer channel

### Build

The library is header only.

To build example programs and the test suite, from the project root directory:

```
mkdir build && cd build
cmake .. -G Ninja -DWMP_BUILD_EXAMPLES=ON -DWMP_BUILD_TESTS=ON
ninja
```

### Testing

The `catch2` unit testing library is used to write tests against `wmp`. Once the test suite is built, run the tests with `ctest`.