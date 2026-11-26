add_executable(test_hdmi main.cpp)
target_link_libraries(test_hdmi PRIVATE hdmi_lib)
target_include_directories(test_hdmi PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_hdmi COMMAND test_hdmi)
set_tests_properties(test_hdmi PROPERTIES TIMEOUT 120)
install(TARGETS test_hdmi RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set_tests_properties(test_hdmi PROPERTIES LABELS "optional-hardware")
