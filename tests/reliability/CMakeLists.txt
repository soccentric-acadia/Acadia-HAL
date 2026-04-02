add_executable(test_reliability main.cpp)
target_link_libraries(test_reliability reliability_lib)
target_include_directories(test_reliability PRIVATE ../src/reliability_lib)
add_test(NAME test_reliability COMMAND test_reliability)
