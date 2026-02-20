add_library(imu_lib imu_lib.cpp imu_lib.h)
target_include_directories(imu_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(imu_lib lib_base)
