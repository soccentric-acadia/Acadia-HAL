add_library(mcu_interface_lib mcu_interface_lib.cpp mcu_interface_lib.h)
target_include_directories(mcu_interface_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mcu_interface_lib lib_base)
