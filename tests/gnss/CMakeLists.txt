add_executable(test_gnss main.cpp)
target_link_libraries(test_gnss gnss_lib)
target_include_directories(test_gnss PRIVATE ../src/gnss_lib)
add_test(NAME test_gnss COMMAND test_gnss)
