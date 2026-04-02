add_library(pcie_lib pcie_lib.cpp pcie_lib.h)
target_include_directories(pcie_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pcie_lib lib_base)
