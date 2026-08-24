add_executable(test_vision_accelerator_metrics main.cpp)
target_link_libraries(test_vision_accelerator_metrics
    PRIVATE
        hal_protos_server  # provides TypedModuleHandler<...> template
        vision_accelerator_lib
)
target_include_directories(test_vision_accelerator_metrics
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME test_vision_accelerator_metrics COMMAND test_vision_accelerator_metrics)
set_tests_properties(test_vision_accelerator_metrics PROPERTIES TIMEOUT 300)
install(TARGETS test_vision_accelerator_metrics RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set_tests_properties(test_vision_accelerator_metrics PROPERTIES LABELS "host")
