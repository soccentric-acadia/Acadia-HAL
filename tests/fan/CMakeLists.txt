add_executable(test_fan main.cpp)
target_link_libraries(test_fan PRIVATE fan_lib)
target_include_directories(test_fan PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_fan COMMAND test_fan)
set_tests_properties(test_fan PROPERTIES TIMEOUT 120)
install(TARGETS test_fan RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set_tests_properties(test_fan PROPERTIES LABELS "target-required")
