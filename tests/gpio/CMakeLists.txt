add_executable(test_gpio main.cpp)
target_link_libraries(test_gpio gpio_lib)
target_include_directories(test_gpio PRIVATE ../src/gpio_lib)
add_test(NAME test_gpio COMMAND test_gpio)
