# CMakeLists.txt

cmake_minimum_required(VERSION 3.16)

project(wmp CXX)

option(WMP_BUILD_EXAMPLES "Build examples" ON)
option(WMP_BUILD_TESTS "Build tests" ON)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(
    ${PROJECT_NAME} 
    INTERFACE 
    $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>)
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)

if(WMP_BUILD_EXAMPLES)
    message("Configuring examples...")
    add_subdirectory(example/)
endif()

if(WMP_BUILD_TESTS)
    message("Configuring tests...")
    enable_testing()
    add_subdirectory(deps/catch2)
    add_subdirectory(test/)
endif()