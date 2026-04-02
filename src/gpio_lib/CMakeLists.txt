add_library(gpio_lib gpio_lib.cpp gpio_lib.h)
target_include_directories(gpio_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gpio_lib lib_base)
