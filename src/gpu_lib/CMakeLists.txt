add_library(gpu_lib gpu_lib.cpp gpu_lib.h)
target_include_directories(gpu_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(gpu_lib lib_base)
