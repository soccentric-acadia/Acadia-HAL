add_executable(test_ethernet main.cpp)
target_link_libraries(test_ethernet ethernet_lib)
target_include_directories(test_ethernet PRIVATE ../src/ethernet_lib)
add_test(NAME test_ethernet COMMAND test_ethernet)
