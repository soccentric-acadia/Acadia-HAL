add_library(ethernet_lib ethernet_lib.cpp ethernet_lib.h)
target_include_directories(ethernet_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ethernet_lib lib_base)
