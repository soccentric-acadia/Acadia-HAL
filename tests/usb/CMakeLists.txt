add_executable(test_usb main.cpp)
target_link_libraries(test_usb usb_lib)
target_include_directories(test_usb PRIVATE ../src/usb_lib)
add_test(NAME test_usb COMMAND test_usb)
