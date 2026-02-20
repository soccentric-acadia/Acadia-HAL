add_executable(test_gpu main.cpp)
target_link_libraries(test_gpu acu_full_test)
target_include_directories(test_gpu PRIVATE ../src)

add_test(NAME test_gpu COMMAND test_gpu)
