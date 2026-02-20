add_library(ethernet_switch_lib ethernet_switch_lib.cpp ethernet_switch_lib.h)
target_include_directories(ethernet_switch_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ethernet_switch_lib lib_base)
