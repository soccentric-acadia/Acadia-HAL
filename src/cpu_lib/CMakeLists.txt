add_library(cpu_lib cpu_lib.cpp cpu_lib.h)
target_include_directories(cpu_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(cpu_lib Threads::Threads)
target_link_libraries(cpu_lib lib_base)
