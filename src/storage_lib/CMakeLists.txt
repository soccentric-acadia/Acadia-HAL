add_library(storage_lib storage_lib.cpp storage_lib.h)
target_include_directories(storage_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(storage_lib lib_base)
