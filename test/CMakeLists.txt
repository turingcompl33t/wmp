# test/CMakeLists.txt

# automatically enable catch2 to generate ctest targets
include(${PROJECT_SOURCE_DIR}/deps/catch2/contrib/Catch.cmake)

add_library(catch_main STATIC catch_main.cpp)
target_link_libraries(catch_main PUBLIC Catch2::Catch2)
#target_link_libraries(catch_main PRIVATE project_options)

set(wmp_test_suite_srcs
    "src/oneshot.cpp")
add_executable(wmp_test_suite ${wmp_test_suite_srcs})
target_link_libraries(wmp_test_suite PRIVATE catch_main wmp)

catch_discover_tests(wmp_test_suite)