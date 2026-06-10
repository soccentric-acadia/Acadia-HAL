add_executable(test_sysfs_helpers main.cpp)
target_link_libraries(test_sysfs_helpers PRIVATE lib_base)
target_include_directories(test_sysfs_helpers
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME test_sysfs_helpers COMMAND test_sysfs_helpers)
set_tests_properties(test_sysfs_helpers PROPERTIES TIMEOUT 30)
install(TARGETS test_sysfs_helpers RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
