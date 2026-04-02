add_library(environmental_lib environmental_lib.cpp environmental_lib.h)
target_include_directories(environmental_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(environmental_lib lib_base)
