# example/CMakeLists.txt

add_executable(oneshot1 "oneshot1.cpp")
target_link_libraries(oneshot1 PRIVATE wmp)

add_executable(oneshot2 "oneshot2.cpp")
target_link_libraries(oneshot2 PRIVATE wmp)

add_executable(mpsc1 "mpsc1.cpp")
target_link_libraries(mpsc1 PRIVATE wmp)

add_executable(watch1 "watch1.cpp")
target_link_libraries(watch1 PRIVATE wmp)