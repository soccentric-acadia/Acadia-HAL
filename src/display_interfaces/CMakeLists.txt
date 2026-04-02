add_library(display_interfaces display_interfaces.cpp display_interfaces.h)
target_include_directories(display_interfaces PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(display_interfaces lib_base)
