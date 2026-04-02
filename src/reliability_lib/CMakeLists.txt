add_library(reliability_lib reliability_lib.cpp reliability_lib.h)
target_include_directories(reliability_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(reliability_lib lib_base)
