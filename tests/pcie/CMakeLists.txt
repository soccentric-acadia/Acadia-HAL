add_executable(test_pcie main.cpp)
target_link_libraries(test_pcie pcie_lib)
target_include_directories(test_pcie PRIVATE ../src/pcie_lib)
add_test(NAME test_pcie COMMAND test_pcie)
