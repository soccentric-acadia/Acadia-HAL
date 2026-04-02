add_library(gnss_lib gnss_lib.cpp gnss_lib.h)
target_include_directories(gnss_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(gnss_lib lib_base)
