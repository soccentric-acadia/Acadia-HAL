add_executable(test_cpu main.cpp)
target_link_libraries(test_cpu acu_full_test)
target_include_directories(test_cpu PRIVATE ../src)
add_test(NAME test_cpu COMMAND test_cpu)
