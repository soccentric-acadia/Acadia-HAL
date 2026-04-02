add_library(system_memory_lib system_memory_lib.cpp system_memory_lib.h)
target_include_directories(system_memory_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(system_memory_lib lib_base)
