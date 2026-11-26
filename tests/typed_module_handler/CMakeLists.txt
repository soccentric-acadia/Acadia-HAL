add_executable(test_typed_module_handler main.cpp)
target_link_libraries(test_typed_module_handler
    PRIVATE
        hal_protos_server  # provides TypedModuleHandler<...> template
)
target_include_directories(test_typed_module_handler
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME test_typed_module_handler COMMAND test_typed_module_handler)
set_tests_properties(test_typed_module_handler PROPERTIES TIMEOUT 300)
install(TARGETS test_typed_module_handler RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set_tests_properties(test_typed_module_handler PROPERTIES LABELS "host")
