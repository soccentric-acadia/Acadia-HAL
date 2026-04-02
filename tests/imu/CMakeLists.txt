add_executable(test_imu main.cpp)
target_link_libraries(test_imu imu_lib)
target_include_directories(test_imu PRIVATE ../src/imu_lib)
add_test(NAME test_imu COMMAND test_imu)
