# example/CMakeLists.txt

add_executable(oneshot1 "oneshot1.cpp")
target_link_libraries(oneshot1 PRIVATE wmp)

add_executable(oneshot2 "oneshot2.cpp")
target_link_libraries(oneshot2 PRIVATE wmp)