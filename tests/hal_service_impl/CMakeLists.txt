add_executable(test_hal_service_impl main.cpp)
target_link_libraries(test_hal_service_impl
    PRIVATE
        hal_protos_server  # provides HalServiceImpl, ModuleRegistry, TypedModuleHandler
)
target_include_directories(test_hal_service_impl
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME test_hal_service_impl COMMAND test_hal_service_impl)
set_tests_properties(test_hal_service_impl PROPERTIES TIMEOUT 60)
install(TARGETS test_hal_service_impl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
